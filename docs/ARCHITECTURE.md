# Architecture

1. The source and store roots are canonicalized and must not overlap.
2. Each CLI operation takes an exclusive advisory flock on the owner-private `.operation.lock`; this prevents SRE commands such as verify and prune from racing each other.
3. Source traversal starts at a directory descriptor and enumerates without following symlinks or crossing the root device. Regular files are opened with openat2 and RESOLVE_BENEATH, RESOLVE_NO_SYMLINKS, and RESOLVE_NO_MAGICLINKS.
4. Each file is copied while hashing into an exclusive temporary blob. A before/after fstat stability check, blob fsync, same-filesystem rename, and shard-directory fsync precede manifest references.
5. SQLite uses WAL and full synchronous mode with foreign-key enforcement. Snapshot rows move from COPYING to COMMITTED only after file entries are complete.
6. Restore plans are read-only in dry-run mode. Confirmed restore verifies the blob, writes a temp file beside the destination, fsyncs, atomically renames, re-hashes, and fsyncs the parent directory.
7. `doctor` marks a leftover COPYING row as FAILED and removes abandoned `.part` files while holding the same store lock.

The manifest stores per-file consistency, not a globally consistent tree snapshot. Content-addressed dedup is used; pruning removes database references first and only deletes blobs with no remaining references. The advisory lock coordinates SRE CLI processes only; external writers may ignore it.
