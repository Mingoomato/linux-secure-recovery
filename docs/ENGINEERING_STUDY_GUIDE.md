# Engineering Study Guide — Linux Secure Recovery Engine

## Learning objective and implementation status

This project teaches local snapshot construction, per-file content integrity, manifest consistency, and safe file-level recovery. It is intended for pre-change snapshots in an isolated lab. A snapshot created after damage cannot recover earlier data; a local store does not protect against root compromise or a shared-disk failure.

The Release build and local recovery CTest smoke passed on the current host, which supports `openat2`. The separate ASan/UBSan smoke also passed. Tests cover snapshot, verification, dry-run, conflict handling, restore, a synthetic stale `COPYING` row, abandoned temp cleanup, and process-level lock waiting. Corrupt-blob injection, abrupt crash/power-loss injection, concurrent source mutation, ENOSPC, overwrite edge cases, performance, and extended static analysis remain unverified. Implementation is partial against SRS-04: restore does not reapply ownership or mtime; quota, complete manifest-sidecar recovery, full dry-run conflict/deletion planning, and crash durability evidence remain incomplete.

## Why each subsystem exists

| Subsystem | Why it exists | Main implementation |
|---|---|---|
| Root validation | Prevents source and store overlap and anchors traversal in a known directory. | `canonical_dir`, `cmd_init`, `cmd_snapshot` |
| Store operation lock | Serializes SRE CLI operations so prune cannot remove blobs during verification or restore. | `StoreLock` using `flock(LOCK_EX)` |
| Constrained path opener | Prevents traversal escape and symlink/magic-link resolution for source entries. | `open_beneath` using `openat2` |
| Content copier and SHA-256 | Streams bytes into a temporary blob while identifying the content and checking source stability. | `save_blob`, `sha256_fd`, `same_state` |
| Content-addressed store | Reuses a digest-named blob when identical bytes recur. | `ensure_store_dirs`, `save_blob`, `blob_path` |
| SQLite manifest | Separates `COPYING` from `COMMITTED` snapshots and links paths/metadata to blob hashes. | `open_db`, `cmd_snapshot`, `load_entries` |
| Verification | Re-hashes stored blobs before relying on them. | `cmd_verify` |
| Safe restore | Stages bytes beside the target, checks hash, then atomically renames per file. | `ensure_parent`, `restore_one`, `cmd_restore` |
| Pruning/audit | Removes old committed snapshot references and collects unreferenced blobs; records operations locally. | `cmd_prune`, `audit` |
| Restart doctor | Marks snapshots left in `COPYING` as failed and removes abandoned `.part` temp files. | `cmd_doctor` |

## Data and control flow

### Snapshot

1. `cmd_snapshot` canonicalizes source and store, rejects overlap, and takes the store's private exclusive advisory lock.
2. It opens the source directory and captures its device number, creates a random snapshot ID, opens SQLite, commits a `COPYING` row, then starts the transaction for entries.
3. `scan_tree` enumerates directories relative to directory FDs, rejects symlinks and device changes, and ignores special files.
4. For each regular file, `save_blob` calls `open_beneath`, samples `fstat`, streams file chunks into a temporary file while hashing, samples `fstat` again, and rejects an unstable source.
5. The blob is fsynced, closed, renamed to `blobs/sha256/<prefix>/<digest>`, and its shard directory is fsynced before a manifest row can refer to it.
6. The DB transaction marks the snapshot committed. A JSON manifest and audit row are then written. If a prior process exited while leaving `COPYING`, `doctor` marks that row failed and removes remaining `.part` temp artifacts; it does not reconstruct missing entry rows.

### Verify and restore

`cmd_verify` reads committed manifest entries and re-hashes each blob. `cmd_restore` first reads and validates entries; dry-run prints the planned file/hash without writing. Confirmed restore creates needed parent directories beneath an opened target root, opens the blob, writes a same-directory temp file, fsyncs it, reopens and hashes the staged file, applies mode, and uses `renameat2` for per-file replacement/no-replace semantics. It verifies the installed inode and fsyncs the parent directory.

## Ownership, lifetime, and concurrency

- `Entry` values own their strings and live in vectors for the duration of each operation. SQLite text is copied using `SQLITE_TRANSIENT` during insert; reads are copied before statement finalization.
- Directory and file FDs are manually managed. `fdopendir` owns a duplicated FD; recursive child FDs are closed after recursion. The OpenSSL `EVP_MD_CTX` is manually freed on success/error paths.
- Temporary blob names use snapshot ID and entry index with `O_EXCL`; restore temporary names use PID plus a monotonic tick and are created in the destination directory.
- `StoreLock` owns one lock FD for an entire command. Its destructor unlocks and closes it on normal return or exception. `flock` is advisory; only programs following this lock protocol are serialized.
- SQLite handles/statements and C++ streams are explicitly closed/finalized; many SQLite return codes in setup/transaction paths are not checked uniformly, so fault-injection review remains important.
- No worker threads are used. SRE CLI processes serialize through one exclusive store lock, but outside programs can still mutate the source or store. The snapshot sees source files at different times, although each copied file is checked individually, so the tree can reflect mixed times.

## Security assumptions and failure modes

The store path and its parent directories are trusted and user-private. `openat2` protects source resolution; restore uses an opened target directory and `*at` operations. `safe_relative_path` rejects absolute paths, empty components, `.` and `..` in manifest data. Symlinks, devices, sockets, FIFOs, and files above 1 GiB are excluded.

Expected failures include unsupported `openat2`, unsafe lock files, lock acquisition errors, unstable source files, read/write errors, no space, fsync/rename failure, manifest DB errors, corrupt/missing blobs, target conflicts, insufficient metadata privileges, and process crashes. Blobs may be left unreferenced after a failure; `doctor` recovers stale `COPYING` rows and `.part` files, but it does not reconcile a DB-committed snapshot whose JSON sidecar write failed. The test suite uses a synthetic stale-state fixture; it has not injected real process, kernel, or power failures.

## Alternative designs

- **Filesystem snapshots** can provide tree-level point-in-time consistency, but are filesystem-specific and require privileged setup.
- **Tar/rsync copies** are simpler and broadly available but do not inherently provide a transactional manifest, hash-addressed deduplication, or atomic per-file recovery guarantees.
- **Remote immutable object storage** protects against local disk/root failure but adds credentials, network retries, retention, and key-management risks.
- **Write-ahead log/journal** or a manifest-first protocol can improve recovery from crashes, but requires a clear state machine for temp blobs, committed rows, manifests, and garbage collection.
- **SQLite transaction locking** coordinates database writers but not blob-file reads and deletion; a shared/exclusive lock protocol could allow concurrent readers, while the current single exclusive `flock` keeps the first implementation easier to reason about.
- **Hard-link deduplication** can reduce space but couples inode metadata and makes immutability assumptions harder to enforce; content-addressed blobs keep data identity separate from destination metadata.

## Study route

Trace `snapshot` through `scan_tree` → `save_blob` → SQLite entry rows; then trace `restore` through `ensure_parent` and `restore_one`. Draw the point at which blob bytes become durable and the point at which the manifest references them. Use `docs/CODE_WALKTHROUGH.md`, `docs/CONSISTENCY_MODEL.md`, and `docs/LINUX_INTERNALS_EXPLAINED.md` before attempting an exercise.

## Official references

- [`openat2(2)`](https://man7.org/linux/man-pages/man2/openat2.2.html), [`openat(2)`](https://man7.org/linux/man-pages/man2/openat.2.html)
- [`flock(2)`](https://man7.org/linux/man-pages/man2/flock.2.html) for process-level advisory locking
- [`fsync(2)`](https://man7.org/linux/man-pages/man2/fsync.2.html), [`rename(2)` / `renameat2`](https://man7.org/linux/man-pages/man2/rename.2.html), [`stat(2)`](https://man7.org/linux/man-pages/man2/stat.2.html)
- [SQLite WAL](https://www.sqlite.org/wal.html), [SQLite Atomic Commit](https://www.sqlite.org/atomiccommit.html), [SQLite foreign-key enforcement](https://www.sqlite.org/foreignkeys.html)
- [OpenSSL EVP digest API](https://docs.openssl.org/3.0/man3/EVP_DigestInit/)
