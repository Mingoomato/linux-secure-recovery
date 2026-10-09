# Test plan

- Verify init overlap rejection, openat2 availability, empty/Unicode/binary/zero-byte/large files, symlinks, and special-file exclusion.
- Snapshot a tree, compare blob hashes, mutate source files, then restore into a separate target and compare bytes.
- Confirm dry-run changes no target file; confirm existing files are skipped unless overwrite is explicit.
- Corrupt a blob and manifest; verify reports corruption and refuses unsafe restore.
- Exercise concurrent source mutation, no-space/I/O failure, crash between blob and manifest commit, unsupported openat2, quota pruning, and referenced-blob retention.
- Run 1 GiB/1k benchmark and report hardware/filesystem details only after measurement.

The local recovery smoke test has been run. Corruption, crash/fault injection, sanitizer, static-analysis, and performance scenarios remain pending.
