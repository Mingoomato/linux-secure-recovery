# Code Walkthrough — Linux Secure Recovery Engine

## 1. CLI, operation lock, and roots

`main` dispatches the SRE commands. Every command that uses a store obtains `StoreLock`, which opens `.operation.lock` with `O_NOFOLLOW`, checks that it is a private owner-owned regular file, and waits for `flock(LOCK_EX)`. RAII releases and closes the lock FD when the command returns. This serializes SRE's own operations; advisory locks do not stop unrelated tools that ignore them. `canonical_dir` rejects a symlink or non-directory input and resolves an existing directory. `cmd_init` and `cmd_snapshot` reject source/store overlap. The source root is opened as a directory FD; later traversal is rooted there rather than using the process working directory.

## 2. Opening source paths

`open_beneath` builds a zero-initialized `open_how`, sets `O_CLOEXEC`, and asks `SYS_openat2` to resolve beneath a root FD without symlinks or magic links. If the syscall is not available, it fails closed. `scan_tree` recursively enumerates a duplicated directory FD, uses `fstatat(..., AT_SYMLINK_NOFOLLOW)`, skips cross-device/symlink entries, opens child directories under the same root, and processes only regular files.

## 3. Blob creation and manifest rows

`save_blob` opens one regular file, records `fstat` state, creates an exclusive temp blob, and streams each read chunk through EVP SHA-256 and `write_all`. It compares post-copy `fstat`; unstable input is rejected. After `fsync`, it derives a SHA-256 path, ensures the shard is a real directory, reuses an existing digest blob or renames the temp blob into place, then fsyncs the shard directory.

`cmd_snapshot` creates a `COPYING` SQLite row in a short transaction, then starts the transaction that contains file entries. `scan_tree` inserts each row only after its blob is finalized. At the end the snapshot state changes to `COMMITTED`, and a JSON manifest and audit line are written. The intended invariant is “durable blob before manifest reference.” The code does not yet provide full crash-recovery tooling for all intermediate states.

## 4. Verification and restore

`load_entries` reads entries and checks each relative path with `safe_relative_path`. `cmd_verify` opens each content-addressed blob without following a final symlink and recalculates its digest.

`cmd_restore` validates the target root and snapshot, then either prints a dry-run plan or requires `--confirm`. `ensure_parent` walks/creates destination directories with `mkdirat` and `openat(O_DIRECTORY|O_NOFOLLOW)`. `restore_one` checks conflict policy, opens the blob, creates a same-directory temp file, copies and fsyncs bytes, reopens and hashes the staged file, applies mode, then uses `renameat2`. It confirms that the installed entry is the staged inode and fsyncs the destination directory. The code restores mode but does not restore ownership or mtime.

## 5. Doctor, pruning, and audit

`cmd_doctor` runs under the same store lock, changes leftover `COPYING` rows to `FAILED` with one incomplete item, and removes regular files or symlinks ending in `.part` from `temp/`. The test seeds this state directly in SQLite; it is not a true process-crash simulation. `cmd_prune` orders committed snapshots by creation time, deletes database rows beyond the retention count, removes JSON manifests, computes the set of still-referenced hashes, and recursively removes unreferenced regular files. Review transaction boundaries and crash behavior: manifest unlink happens before the SQLite transaction commits, and GC is not fault-injection tested. `audit` appends operation ID/count/confirmation metadata to a local NDJSON file.

## 6. Ownership and failure review

The code uses raw file descriptors and explicit cleanup. For each error branch, track `rootfd`, source `src`, temp `dst`, reopened `staged_fd`, `dirfd`, `sqlite3*`, `sqlite3_stmt*`, and `EVP_MD_CTX*`. C++ RAII protects strings, vectors, streams, and filesystem paths, but it does not automatically close these C resources. Current manual cleanup is extensive but lacks a uniform scope guard; injected failures are needed to verify every path.
