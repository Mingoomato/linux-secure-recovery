# Consistency model

- Each regular file is a stable-read candidate. Metadata is sampled before and after the content copy. A mismatch aborts the snapshot rather than recording a mixed file.
- Blob bytes are committed before SQLite manifest entries reference them.
- A snapshot can contain files observed at different moments. The consistency marker is per_file_verified.
- A process crash before the manifest transaction commits can leave an unreferenced blob or a COPYING row. Previously committed snapshots remain available.
- Restore is atomic per file on the destination filesystem when renameat2 is supported. Tree-wide atomicity is not claimed.
- A dry run performs no destination writes. Confirmed restore writes only files listed in the selected snapshot and never removes other paths.
