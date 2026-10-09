# Consistency model

- Each regular file is a stable-read candidate. Metadata is sampled before and after the content copy. A mismatch aborts the snapshot rather than recording a mixed file.
- Blob bytes are committed before SQLite manifest entries reference them.
- A snapshot can contain files observed at different moments. The consistency marker is per_file_verified.
- A process crash before the manifest transaction commits can leave an unreferenced blob or a COPYING row. Previously committed snapshots remain available.
- Every CLI operation takes the per-store exclusive `flock`, so SRE's own verify, restore, prune, and doctor operations do not overlap. `flock` is advisory: direct filesystem/SQLite access or other programs can ignore it.
- `doctor` recovers rows still in COPYING by marking them FAILED with `incomplete_count=1`, and removes regular files or symlinks ending in `.part` from the store temp directory. It does not reconstruct a snapshot, verify a committed sidecar, or prove power-loss durability.
- The recovery test seeds a synthetic COPYING row and temp artifacts. It verifies application state transitions, not abrupt process termination, kernel crash, or storage-device behavior.
- Restore is atomic per file on the destination filesystem when renameat2 is supported. Tree-wide atomicity is not claimed.
- A dry run performs no destination writes. Confirmed restore writes only files listed in the selected snapshot and never removes other paths.
