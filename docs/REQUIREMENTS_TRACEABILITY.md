# Requirements traceability (SRS-04)

The smoke test covers local snapshot, verification, dry-run, conflict handling, restore, synthetic stale-COPYING recovery, temp cleanup, and inter-process lock waiting. Real crash, corruption, and storage-fault scenarios remain unverified. See `docs/TEST_RESULTS.md`.

| Requirement | Source / artifact | Verification | Status |
|---|---|---|---|
| SRE-FR-001 | canonical root overlap checks | SRE-T-001 pending | IMPLEMENTED |
| SRE-FR-002 | openat2 source resolution | SRE-T-003 pending | IMPLEMENTED |
| SRE-FR-003 | traversal regular files only | SRE-T-002/003 pending | IMPLEMENTED |
| SRE-FR-004 | SQLite entry metadata and SHA-256 | tests/smoke.sh (single-file snapshot) | IMPLEMENTED |
| SRE-FR-005 | save_blob pre/post fstat | SRE-T-004 pending | IMPLEMENTED |
| SRE-FR-006 | temp/fsync/rename/shard fsync | SRE-T-005 pending | IN_PROGRESS |
| SRE-FR-007 | COPYING/COMMITTED snapshot transaction; doctor marks stale COPYING failed | tests/smoke.sh synthetic recovery | IN_PROGRESS |
| SRE-FR-008 | snapshots/entries tables + JSON manifest | manifest review pending | IN_PROGRESS |
| SRE-FR-009 | verify blob SHA-256 | tests/smoke.sh (blob verification) | IMPLEMENTED |
| SRE-FR-010 | restore dry-run plan | tests/smoke.sh (dry-run/conflict) | IMPLEMENTED |
| SRE-FR-011 | confirm and overwrite flags | SRE-T-008 pending | IMPLEMENTED |
| SRE-FR-012 | temp file, fsync, rename, post-hash | tests/smoke.sh (restore bytes) | IN_PROGRESS |
| SRE-FR-013 | file mode restored; other metadata omitted | SRE-T-009 pending | IN_PROGRESS |
| SRE-FR-014 | restore never deletes tree | SRE-T-008 pending | IMPLEMENTED |
| SRE-FR-015 | prune references then collect blobs | SRE-T-010 pending | IN_PROGRESS |
| SRE-FR-016 | incomplete rows and temp behavior; doctor cleans abandoned `.part` files | tests/smoke.sh synthetic recovery and temp cleanup | IN_PROGRESS |
| SRE-FR-017 | local operation audit log | audit review pending | IMPLEMENTED |
| SRE-FR-018 | CLI dispatch | CLI smoke pending | IN_PROGRESS |
| SRE-FR-019 | content-addressed dedup | SRE-T-010 pending | IN_PROGRESS |
| SRE-FR-020 | file size cap | SRE-T-013 pending | IN_PROGRESS |
| SRE-FR-021 | — | optional, not implemented | NOT_APPLICABLE |
| SRE-NFR-01..07 | docs/TEST_PLAN.md | not run | NOT_STARTED |
