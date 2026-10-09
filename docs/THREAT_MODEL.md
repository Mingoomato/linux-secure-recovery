# Threat model

## Protected assets

Committed snapshots and their regular-file content blobs are protected against accidental partial writes and detectable corruption.

## Controls

- Source/store overlap is rejected.
- Source traversal rejects symlinks, mount crossings, special files, unstable reads, and files larger than 1 GiB.
- openat2 path resolution fails closed on unsupported kernels.
- Blob creation uses exclusive temp files, file fsync, atomic rename, and directory fsync.
- Restore requires --confirm for writes, uses --overwrite to replace, never deletes the tree, and verifies restored hashes.
- SQLite metadata uses transactions, WAL, and FULL synchronous mode.

## Residual risks

The store is not immutable against root or the same owner, and a common disk failure can destroy source and backup together. The JSON manifest/audit logs are not signed. Existing-file replacement is atomic but does not preserve the replaced version unless it was previously snapshotted. Directories, ACLs, xattrs, hardlink identity, ownership, and global filesystem snapshot consistency are not preserved. Crash recovery and durability assumptions require VM fault testing.
