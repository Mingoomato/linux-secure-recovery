# Technical Defense — Linux Secure Recovery Engine

Questions only; no answers are provided. Be precise about file-level guarantees versus tree-wide claims.

## Code reading and data flow

1. Trace one source path from `scan_tree` through `open_beneath`, `save_blob`, and the SQLite `entries` row.
2. What does `RESOLVE_BENEATH` guarantee, and why are `RESOLVE_NO_SYMLINKS` and `RESOLVE_NO_MAGICLINKS` explicit?
3. Which fields does `same_state` compare? Which possible content changes can evade that check?
4. At what point does the blob become durable, and when may a manifest row reference it?
5. Explain the `COPYING` to `COMMITTED` state transition and where the JSON sidecar is written.
6. How does `cmd_verify` establish whether a content-addressed blob matches its manifest hash?
7. Trace `restore --dry-run` and `restore --confirm` and list which path each is allowed to write.
8. Why does restore stage a file in the destination directory before `renameat2`?

## Debugging and failure behavior

9. What happens if the source changes while `save_blob` is reading it?
10. A snapshot has a `COPYING` row after a crash. Which durable objects may exist, and which states must not be assumed?
11. How are `EINTR`, `ENOSPC`, `EIO`, fsync failure, and rename failure handled in the current code?
12. Why does the existing smoke test not prove crash recovery or no-space correctness?
13. What can happen if SQLite commits but the JSON manifest sidecar write fails?
14. How does `cmd_prune` avoid deleting a blob still referenced by another snapshot? What transaction-order issue would you inspect?
15. What exact work is performed when `doctor` encounters a stale `COPYING` row, and which crash cases remain outside its recovery logic?
16. Why does `doctor` remove `.part` names with `symlink_status`, and what would change if it followed the link?
17. Which command holds the store lock while reading blobs, and what race would be possible if prune held the lock alone?

## Security reasoning

18. Why are source/store overlap and symlink rejection important?
19. What does `safe_relative_path` protect against in a damaged or malicious manifest?
20. Why are `O_EXCL`, `O_NOFOLLOW`, and directory FDs used for temporary/destination files?
21. What can a root attacker or common-disk failure do to this local store?
22. Why does a content hash not authenticate a snapshot manifest?
23. Which metadata is restored today, and what are the security/permission implications of restoring UID/GID?
24. `flock` is advisory: which actors can bypass this serialization, and how does the RAII lock FD lifetime release it?
25. Why does `StoreLock` reject a symlink, a non-regular file, or a lock inode with multiple hard links?

## Design tradeoffs

26. Compare per-file atomic rename, whole-tree staging, and filesystem snapshots.
27. How should quota and garbage collection interact with a snapshot currently being written?
28. Which consistency state would you add for a DB-committed snapshot whose JSON sidecar is missing, and how would you recover it safely?
29. What durability does file `fsync` provide, and why is parent-directory `fsync` also required after rename?
30. Which next gate gives the strongest evidence: corruption injection, crash injection, concurrent mutation, or cross-filesystem testing?

Owner mastery requires direct evidence from an independently completed exercise and reasoned answers; automated test results alone do not establish it.
