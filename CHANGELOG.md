# Changelog

## 0.1.0 — 2026-10-10

- SRS-04 기반 교육용 snapshot, verify, restore, retention, recovery CLI를 추가했습니다.
- `manifest.v1` canonical sidecar와 자체 integrity hash, `operation.v1` audit schema를 정의했습니다.
- corrupt blob/manifest 거부, metadata 복원, 상세 show 조회, shared-blob prune, orphan manifest recovery를 검증했습니다.
- GCC/Clang/ASan/UBSan과 로컬 smoke, 정적 분석을 실행했습니다.
- SRS 전체 완료, crash durability, quota, 성능 또는 오너 숙련도는 주장하지 않습니다. [`docs/TEST_RESULTS.md`](docs/TEST_RESULTS.md) 참조.
