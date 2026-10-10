# SRE 검증 계획

## 완료된 로컬 게이트

- GCC Release, Clang Release, ASan/UBSan Debug 빌드와 CTest.
- `safe_relative_path`/`safe_snapshot_id`의 경계 단위 테스트 및 Clang libFuzzer 10,000-run smoke.
- source/store가 같거나 서로 포함되는 경로를 거부하고, 거부 전에 하위 store 디렉터리를 만들지 않는 확인.
- snapshot/show/verify, dry-run, 기존 대상 충돌, 파일 복원 및 mode/mtime 확인.
- schema형 dry-run에서 생성, 정규 파일/symlink/directory 보존, 정규 파일/symlink 교체, overwrite 대상 directory 차단 및 target no-write/delete=0 확인.
- blob 및 sidecar 변조, 확인된 복원 거부와 기존 대상 보존.
- audit NDJSON schema, 파일 mode, affected/byte count, prune의 공유 blob 보존.
- 합성 중단 상태 및 고아 파일에 대한 `doctor`, 두 프로세스의 advisory lock 동작.
- Clang static analyzer, JSON Schema validation, `git diff --check`.

## 후속 승격 게이트

1. 전체 특수 항목 행렬, symlink parent, zero-byte/Unicode/binary/1 GiB 경계 테스트 및 제외 항목 기록.
2. snapshot 도중 source mutation, ENOSPC/EIO/EACCES와 fsync/rename fault injection.
3. 실제 강제 종료 지점을 단계별로 주입하고 이전 committed snapshot의 유지 여부를 확인.
4. dry-run 계획 이후 overwrite race, restore 후 설치된 target hash를 검증.
5. quota/retention 경쟁과 여러 참조 snapshot에서의 GC 불변성 검사.
6. 1,000개/1 GiB benchmark와 CPU/RSS/I/O/처리량 기록.
7. 두 배포판, 여러 파일시스템과 마운트 옵션에서 crash/durability 가정을 검증.
8. fault matrix 결과와 각 SRS MUST의 pass/fail 증거를 문서화.

현재 계획의 미실행 항목은 완료된 것으로 보고하지 않습니다. 상세 결과는 [`TEST_RESULTS.md`](TEST_RESULTS.md)에 있습니다.
