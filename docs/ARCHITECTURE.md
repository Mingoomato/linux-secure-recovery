# Architecture

1. The source and store roots are canonicalized and must not overlap.
2. Source traversal starts at a directory descriptor and enumerates without following symlinks or crossing the root device. Regular files are opened with openat2 and RESOLVE_BENEATH, RESOLVE_NO_SYMLINKS, and RESOLVE_NO_MAGICLINKS.
3. Each file is copied while hashing into an exclusive temporary blob. A before/after fstat stability check, blob fsync, same-filesystem rename, and shard-directory fsync precede manifest references.
4. SQLite uses WAL and full synchronous mode. Snapshot rows move from COPYING to COMMITTED only after file entries are complete.
5. Restore plans are read-only in dry-run mode. Confirmed restore verifies the blob, writes a temp file beside the destination, fsyncs, atomically renames, re-hashes, and fsyncs the parent directory.

The manifest stores per-file consistency, not a globally consistent tree snapshot. Content-addressed dedup is used; pruning removes database references first and only deletes blobs with no remaining references.
