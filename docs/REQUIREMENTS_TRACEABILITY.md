# SRS-04 요구사항 추적표

## 별도 상태

- 구현 상태: **PARTIAL** — 명령과 핵심 경로는 동작하지만 모든 MUST 요구사항/완료 DoD를 만족하지 않습니다.
- 기술 검증 상태: **로컬 검증 통과, 전체 SRS 게이트 미완료** — [`TEST_RESULTS.md`](TEST_RESULTS.md).
- 오너 학습/재현 상태: **미평가** — 독립 구현이나 기술 방어 증거 없음.

| 요구사항 | 구현/증거 | 기술 상태 |
|---|---|---|
| SRE-FR-001 | `canonical_dir`, source/store 중첩 거부 | 구현됨; symlink root는 smoke, overlap 조합은 별도 시험 필요 |
| SRE-FR-002 | `open_beneath`와 `openat2` resolve 제한 | 구현됨; symlink traversal 전체 매트릭스는 미실행 |
| SRE-FR-003 | 일반 파일만 포함, symlink/특수/다른 device 제외 | Unicode 이름과 invalid UTF-8 실패, symlink root 확인; 전체 항목 행렬은 미실행 |
| SRE-FR-004 | SHA-256, size, mode, uid/gid, mtime_ns, stable SQLite fields | 구현·smoke 확인 |
| SRE-FR-005 | 복사 전후 `fstat` 비교 | 구현됨; 동시 변경 fault test 미실행 |
| SRE-FR-006 | temp, file fsync, rename, shard fsync | 구현됨; crash/device durability matrix 미실행 |
| SRE-FR-007 | `COPYING → VERIFIED → COMMITTED`, `doctor` stale-state 회수 | 합성 `COPYING`/`VERIFIED` 회수 확인; 실제 crash 미검증 |
| SRE-FR-008 | `manifest.v1`, DB와 canonical sidecar 일치 및 해시 | schema·무결성 hash smoke 통과 |
| SRE-FR-009 | blob hash/크기와 sidecar 정합성 검증 | corrupt blob/manifest 각 restore 거부 확인 |
| SRE-FR-010 | dry-run 경로/digest 출력 | 기본 동작 확인; 충돌·생성·삭제 정책 전체 사전 계획은 미구현 |
| SRE-FR-011 | `--confirm`, `--overwrite`, 기존 파일 기본 보호 | 기본 conflict 및 confirm restore smoke; confirm 누락 전용 시나리오 보강 필요 |
| SRE-FR-012 | 같은 디렉터리 temp, 해시 확인, `renameat2`, directory fsync | bytes 복원 확인; 설치 후 재해시/경합 매트릭스는 미검증 |
| SRE-FR-013 | mode, uid/gid, mtime 복원 시도와 partial 결과 | mode/mtime smoke 확인; UID/GID 특권 경계 미시험 |
| SRE-FR-014 | 선택된 경로만 설치하고 unrelated target 삭제 없음 | 코드 검토; 별도 target-wide 삭제 회귀 테스트 필요 |
| SRE-FR-015 | retention prune, DB 참조 기반 blob GC, 공유 blob 보존 | shared-blob smoke 통과; quota 미구현 |
| SRE-FR-016 | 실패 상태·temp 정리, sidecar recovery | synthetic 상태 및 고아 sidecar smoke; crash/ENOSPC/EIO 주입 미실행 |
| SRE-FR-017 | owner-private NDJSON, operation/count/bytes/confirmed | schema·mode·byte/count smoke 통과; 로그 변조 방지/서명은 없음 |
| SRE-FR-018 | init/snapshot/list/show/verify/restore/prune/doctor | 각 CLI 구현, snapshot/show smoke; 개별 인자 경계 전체 미검증 |
| SRE-FR-019 | 콘텐츠 주소 dedup, 참조 blob 확인 | 두 snapshot과 prune 후 공유 blob 유지 확인; benchmark 미실행 |
| SRE-FR-020 | 1 GiB 파일 상한 | 코드에 상한; 경계 크기 테스트 및 timer/exclude 옵션 미구현 |
| SRE-FR-021 | 압축·암호화·원격 저장은 MAY | 선택 기능, 미구현은 수용 범위 밖 |
| SRE-NFR-01..07 | fixture 복원, 100 crash point, 성능, 다중 배포판 | 일부 로컬 동작만 확인. NFR 완료로 판정하지 않음 |

전체 검증 계획과 미실행 항목은 [`TEST_PLAN.md`](TEST_PLAN.md), 실행 증거는 [`TEST_RESULTS.md`](TEST_RESULTS.md)를 참고하세요.
