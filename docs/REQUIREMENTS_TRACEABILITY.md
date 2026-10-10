# SRS-04 요구사항 추적표

## 별도 상태

- 구현 상태: **PARTIAL** — 명령과 핵심 경로는 동작하지만 모든 MUST 요구사항/완료 DoD를 만족하지 않습니다.
- 기술 검증 상태: **이전 revision 로컬 검증 기록 있음; 현재 변경 revision 미검증** — [`TEST_RESULTS.md`](TEST_RESULTS.md). 사용자 지시에 따라 directory/exclusion 변경 후 검증을 중단했습니다.
- 오너 학습/재현 상태: **미평가** — 독립 구현이나 기술 방어 증거 없음.

| 요구사항 | 구현/증거 | 기술 상태 |
|---|---|---|
| SRE-FR-001 | `canonical_dir`, source/store 중첩 거부 | symlink root와 동일/상위/하위 store overlap 거부를 smoke로 확인. 하위 store는 거부 전에 디렉터리를 만들지 않음. race·mount matrix는 미실행 |
| SRE-FR-002 | `open_beneath`와 `openat2` resolve 제한 | 구현됨; symlink traversal 전체 매트릭스는 미실행 |
| SRE-FR-003 | 일반 파일·디렉터리 트리/빈 디렉터리 지원; symlink/특수/다른 device는 contents 제외 후 path/type/reason 기록 | `scan_tree`, `directory_entries`, `excluded_entries`, `manifest_document`, `restore_directory` 구현 추가. 이번 변경 뒤 전체 항목 행렬은 실행하지 않음 |
| SRE-FR-004 | SHA-256, size, mode, uid/gid, mtime_ns, per-file observed_at_ns, stable SQLite fields | per-file time capture, legacy DB column migration, manifest field 구현; 이번 변경 후 미검증 |
| SRE-FR-005 | 복사 전후 `fstat` 비교 | 구현됨; 동시 변경 fault test 미실행 |
| SRE-FR-006 | temp, file fsync, rename, shard fsync | 구현됨; crash/device durability matrix 미실행 |
| SRE-FR-007 | `COPYING → VERIFIED → COMMITTED`, `doctor` stale-state 회수 | 합성 `COPYING`/`VERIFIED` 회수 확인; 실제 crash 미검증 |
| SRE-FR-008 | `manifest.v1`, DB와 canonical sidecar 일치 및 해시 | schema·무결성 hash smoke 통과 |
| SRE-FR-009 | blob hash/크기와 sidecar 정합성 검증 | corrupt blob/manifest 각 restore 거부 확인 |
| SRE-FR-010 | schema형 JSONL dry-run 계획: 파일 및 디렉터리 생성/보존/교체/차단 및 delete=0 | 기존 파일 계획은 이전 revision에서 smoke/schema 확인; directory plan 구현 추가 후 미검증. 계획 이후 경합과 fault gate도 별도 |
| SRE-FR-011 | `--confirm`, `--overwrite`, 기존 파일 기본 보호 | `--overwrite`만 주고 `--confirm`을 생략하면 exit 2이며 기존 sentinel이 그대로임을 smoke로 확인 |
| SRE-FR-012 | 같은 디렉터리 temp, 해시 확인, `renameat2`, 설치 후 재해시, directory fsync | staged hash 뒤 rename하고 installed path를 nofollow로 다시 열어 inode/size/hash를 확인한 뒤 parent fsync; 이 변경 뒤 fault/경합 시험은 미실행 |
| SRE-FR-013 | mode, uid/gid, mtime 복원 시도와 partial 결과 | source와 restore의 mode·uid·gid·mtime equality를 smoke로 확인; 타 UID 소유권 복원 및 권한 실패/partial matrix는 미검증 |
| SRE-FR-014 | 선택된 경로만 설치하고 unrelated target 삭제 없음 | restore 뒤 manifest 밖 unrelated sentinel이 보존됨을 smoke로 확인 |
| SRE-FR-015 | retention prune, DB 참조 기반 blob GC, 공유 blob 보존 | shared-blob smoke 통과; quota 미구현 |
| SRE-FR-016 | 실패 상태·temp 정리, sidecar recovery | synthetic 상태 및 고아 sidecar smoke; crash/ENOSPC/EIO 주입 미실행 |
| SRE-FR-017 | owner-private NDJSON, operation/count/bytes/confirmed, snapshot excluded_count | 이전 revision audit schema·mode·byte/count 확인; 새 excluded_count field 추가 후 schema validation 미실행. 로그 변조 방지/서명은 없음 |
| SRE-FR-018 | init/snapshot/list/show/verify/restore/prune/doctor | 각 CLI 구현, snapshot/show smoke; 개별 인자 경계 전체 미검증 |
| SRE-FR-019 | 콘텐츠 주소 dedup, 참조 blob 확인 | 두 snapshot과 prune 후 공유 blob 유지 확인; benchmark 미실행 |
| SRE-FR-020 | per-user status, 1 GiB file cap, extension exclusion, timer example | cap, `--exclude-extension`, `show` counts, per-user systemd service/timer example 구현. 새 filter/timer 변경 후 검증하지 않음 |
| SRE-FR-021 | 압축·암호화·원격 저장은 MAY | 선택 기능, 미구현은 수용 범위 밖 |
| SRE-T-012 | 경로 검증 parser fuzzing | Clang libFuzzer + ASan/UBSan 10,000-run smoke 통과; 긴 fuzz 캠페인·재현 corpus 확대는 미실행 |
| SRE-NFR-01..07 | fixture 복원, 100 crash point, 성능, 다중 배포판 | 일부 로컬 동작만 확인. crash point, NFR 성능·배포판 게이트를 완료로 판정하지 않음 |

전체 검증 계획과 미실행 항목은 [`TEST_PLAN.md`](TEST_PLAN.md), 실행 증거는 [`TEST_RESULTS.md`](TEST_RESULTS.md)를 참고하세요.
