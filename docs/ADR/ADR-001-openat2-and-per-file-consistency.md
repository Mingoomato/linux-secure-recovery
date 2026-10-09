# ADR-001: openat2 and per-file consistency

- Status: accepted for the first implementation.
- Decision: require openat2 with strict resolution flags and mark snapshots per_file_verified.
- Reason: string-only path fallback would weaken the path boundary, and a sequential walk cannot claim tree-wide snapshot consistency.
- Consequence: older kernels fail closed; database-like workloads requiring a common point in time need a filesystem snapshot backend.
