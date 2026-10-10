# Changelog

## Unreleased

- `init --max-store-bytes`에 persistent content-blob quota와 dedup-aware enforcement를 추가했습니다. 최신 quota 및 restore target identity recheck 변경은 아직 기술 검증하지 않았습니다.
- 복원 설치 직전에 대상 `(device,inode,type)`을 재확인해 감지 가능한 변경을 거부합니다. 마지막 검사와 rename 사이의 외부 writer 경합은 남습니다.
- `restore --dry-run`에 schema 검증 가능한 항목별 생성/보존/교체/차단 계획과 요약을 추가했습니다. target에 쓰거나 삭제하지 않으며, 실제 적용 전까지 경쟁 상태가 바뀔 수 있음을 문서화했습니다.
- GCC/Clang Release, ASan/UBSan Debug, Clang 정적 분석 및 혼합 충돌 계획 회귀를 통과했습니다.

## 0.1.0 — 2026-10-10

- SRS-04 기반 교육용 snapshot, verify, restore, retention, recovery CLI를 추가했습니다.
- `manifest.v1` canonical sidecar와 자체 integrity hash, `operation.v1` audit schema를 정의했습니다.
- corrupt blob/manifest 거부, metadata 복원, 상세 show 조회, shared-blob prune, orphan manifest recovery를 검증했습니다.
- GCC/Clang/ASan/UBSan과 로컬 smoke, 정적 분석을 실행했습니다.
- SRS 전체 완료, crash durability, filesystem quota/디스크 여유 공간, 성능 또는 오너 숙련도는 주장하지 않습니다. [`docs/TEST_RESULTS.md`](docs/TEST_RESULTS.md) 참조.
