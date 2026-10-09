# Linux Internals Explained — Linux Secure Recovery Engine

## `openat2` path resolution

`openat2` extends `openat` with a zero-initialized `struct open_how` and `resolve` flags. SRE uses `RESOLVE_BENEATH` to prevent resolution from escaping the directory FD, `RESOLVE_NO_SYMLINKS` to reject symbolic links anywhere in the relative path, and `RESOLVE_NO_MAGICLINKS` explicitly to reject procfs-style magic links. Since libc may not provide a wrapper, the code calls `syscall(SYS_openat2, ...)`. Unsupported kernels fail closed rather than falling back to a weaker path open.

The project also checks `st_dev` to skip a mount with a different device. That does not necessarily detect every same-device bind mount; the chosen approach is a simple lab boundary, not a complete mount-namespace policy.

## Stable reads and content identity

The source FD identifies an opened file object. `fstat` before and after a read compares device, inode, size, and nanosecond mtime. This detects many concurrent modifications but cannot provide a globally consistent tree snapshot. SHA-256 identifies the copied bytes; it does not authenticate who supplied them or make the manifest immutable.

## File and directory durability

The blob is written to a same-store temporary file opened with `O_EXCL`, then `fsync` is called on the file before `rename`. The shard directory is fsynced so the new name is durably recorded. Restore similarly creates a temporary file inside the destination directory, writes and fsyncs it, checks its digest, renames it into place, and fsyncs the parent directory. Rename is atomic for one directory entry on the same filesystem; it does not make a multi-file restore atomic.

When overwrite is disabled, `renameat2(..., RENAME_NOREPLACE)` prevents replacing a target that appears after the earlier conflict check. When overwrite is enabled, flags are zero and rename may atomically replace the final path. Intermediate target directories are opened with `O_NOFOLLOW`, reducing symlink traversal.

## SQLite and crash ordering

SQLite WAL and `synchronous=FULL` provide DB transaction behavior; they do not coordinate automatically with blob files or JSON manifest files. SRE deliberately makes blob content durable before inserting references. Crash states can still include an orphan blob, a `COPYING` row, or a DB-committed snapshot whose sidecar manifest was not written. A recovery procedure should inspect state and verify references before garbage collection.

The connection enables `PRAGMA foreign_keys=ON`, so deleting a snapshot can cascade to its entry rows; SQLite does not enable foreign-key enforcement by default on every connection. The code also uses a per-store `flock(LOCK_EX)` file. Linux `flock` is advisory and associated with an open file description: an SRE process waits for other SRE processes using the same lock file, while unrelated code can ignore it. The lock FD's lifetime spans the command and ends when the RAII owner unlocks/closes it.

`doctor` handles two recognizable leftovers under that lock: database rows still marked `COPYING` become `FAILED` with `incomplete_count=1`, and `.part` artifacts are removed from `temp/`. This is a conservative cleanup state transition, not reconstruction. A committed SQLite row with a missing JSON sidecar, a valid-looking but unreferenced blob, or device-level reordering still needs separate reconciliation and fault-injection evidence.

## Failure/resource lifetime

`read` may return fewer bytes than requested or `EINTR`; `write_all` retries interruptions and partial writes. `ENOSPC`, `EIO`, fsync failure, rename failure, and close errors require explicit treatment. Each FD must be closed once on all branches. A source mutation should abort the snapshot rather than claim a stable hash. A corrupt blob must fail verification or staged-restore hash validation.

## Official references

- [`openat2(2)`](https://man7.org/linux/man-pages/man2/openat2.2.html) and [`path_resolution(7)`](https://man7.org/linux/man-pages/man7/path_resolution.7.html)
- [`flock(2)`](https://man7.org/linux/man-pages/man2/flock.2.html) and [SQLite foreign-key support](https://www.sqlite.org/foreignkeys.html)
- [`fsync(2)`](https://man7.org/linux/man-pages/man2/fsync.2.html), [`rename(2)`](https://man7.org/linux/man-pages/man2/rename.2.html), [`openat(2)`](https://man7.org/linux/man-pages/man2/openat.2.html)
- [SQLite WAL](https://www.sqlite.org/wal.html) and [SQLite Atomic Commit](https://www.sqlite.org/atomiccommit.html)
- [OpenSSL EVP digest API](https://docs.openssl.org/3.0/man3/EVP_DigestInit/)
