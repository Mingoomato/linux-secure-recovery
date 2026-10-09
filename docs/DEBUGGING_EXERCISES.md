# Debugging and Independent Reproduction Exercises

These exercises contain no solutions. Use throwaway source/store/target directories. Never test destructive recovery against valuable data.

## Exercise 1 — Corrupt blob and prove restore fails closed

**Goal:** Verify that a corrupted content object cannot be installed at the restore target.

**Starting points:** `cmd_verify`, `blob_path`, `restore_one`, staged-file SHA-256 check, and `tests/smoke.sh`.

Create a snapshot, alter one blob byte in a temporary store, then run verify and confirmed restore into an empty temporary target. Capture exit status, JSON/diagnostic output, and target contents. Add an automated regression test for both verification and restore.

**Constraints:** Use only temporary local files. Do not overwrite an existing destination. Preserve the original blob bytes for post-test cleanup.

**Acceptance:** Neither the bad bytes nor an incomplete file become a successful restore, and the failure is attributable to the selected snapshot entry.

## Exercise 2 — Restore path and symlink safety

**Goal:** Reproduce path-traversal and target-symlink boundary behavior.

**Starting points:** `safe_relative_path`, `ensure_parent`, `restore_one`, `open_beneath`.

Test entries with `../`, repeated separators, absolute path, a symlinked parent, a symlink final component, nested directories, a backslash in a filename, and a target path that shares a textual prefix with another directory. Add a controlled manifest test seam if needed.

**Constraints:** Keep all paths inside temporary roots. Do not weaken `O_NOFOLLOW` or `openat2` constraints to make a test pass.

**Acceptance:** No entry escapes the opened target root; safe nested paths still restore correctly; every rejected case is visible.

## Exercise 3 — Prune and crash-consistency proof

**Goal:** Verify deduplicated blobs survive while referenced and are collected only after the final reference disappears.

**Starting points:** `cmd_prune`, `snapshots`/`entries` schema, manifest unlink/SQLite commit order, content-addressed blob names.

Create two snapshots that share a file, prune one, and verify the remaining snapshot. Then prune all user snapshots and check that metadata, sidecar manifests, and blob files agree. Add fault injection around DB commit and manifest removal; document recovery states.

**Constraints:** Use disposable storage. Do not manually delete the SQLite database. Never claim all-or-nothing semantics without test evidence.

**Acceptance:** No referenced blob is removed; orphan cleanup is repeatable; a simulated interruption leaves a state that `doctor` or documented recovery can identify safely.

## Owner attempt record

Record the selected exercise, state transitions, test evidence, and unresolved risk. The repository does not infer owner mastery; personal reproduction status is **not yet evidenced**.
