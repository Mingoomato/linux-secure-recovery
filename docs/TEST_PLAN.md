# SRE 검증 계획

현재 revision은 directory/exclusion manifest, extension filter, directory restore plan/metadata, content quota, restore target identity recheck 변경 후 검증하지 않았습니다. 아래 완료 항목은 변경 전 revision의 기록이며 이번 코드 상태를 통과로 표시하지 않습니다.

## 완료된 로컬 게이트

- GCC Release, Clang Release, ASan/UBSan Debug 빌드와 CTest.
- `safe_relative_path`/`safe_snapshot_id`의 경계 단위 테스트 및 Clang libFuzzer 10,000-run smoke.
- source/store가 같거나 서로 포함되는 경로를 거부하고, 거부 전에 하위 store 디렉터리를 만들지 않는 확인.
- snapshot/show/verify, dry-run, 기존 대상 충돌, 파일 복원 및 mode/mtime 확인.
- 이전 revision의 schema형 dry-run에서 파일 생성/보존/교체/차단과 target no-write/delete=0 확인. 새 directory create/preserve plan은 아직 검증하지 않음.
- blob 및 sidecar 변조, 확인된 복원 거부와 기존 대상 보존.
- audit NDJSON schema, 파일 mode, affected/byte count, prune의 공유 blob 보존.
- 합성 중단 상태 및 고아 파일에 대한 `doctor`, 두 프로세스의 advisory lock 동작.
- Clang static analyzer, JSON Schema validation, `git diff --check`.

## 후속 승격 게이트

1. 전체 특수 항목 행렬, directory create/preserve/blocked 계획, 새로 생성한 nested/empty directory의 mode·uid/gid·mtime, 기존 directory metadata 보존, 불가능한 chown의 partial 결과, legacy path-only row/manifest 호환성, directory 교체 경합에서 inode 불일치 거부, symlink parent, zero-byte/Unicode/binary/1 GiB 경계, extension filter와 제외 항목 기록, 새 DB와 legacy DB의 `observed_at_ns` 호환성을 검증.
2. snapshot 도중 source mutation, ENOSPC/EIO/EACCES와 fsync/rename fault injection.
3. 실제 강제 종료 지점을 단계별로 주입하고 이전 committed snapshot의 유지 여부를 확인.
4. dry-run 계획 이후 overwrite race, installed path 재개방 및 inode/size/hash post-check를 검증.
5. quota/retention 경쟁과 여러 참조 snapshot에서의 GC 불변성 검사.
6. 1,000개/1 GiB benchmark와 CPU/RSS/I/O/처리량 기록.
7. 두 배포판, 여러 파일시스템과 마운트 옵션에서 crash/durability 가정을 검증.
8. fault matrix 결과와 각 SRS MUST의 pass/fail 증거를 문서화.
9. 새 store quota migration, 기존 usage 계산, 중복 blob 허용, 새 고유 blob의 경계값/초과 거부, legacy store의 unlimited 기본값을 검증.
10. restore 시작 시 관찰한 target inode가 staged file 설치 직전에 바뀌었을 때 기존/경쟁 파일 보존과 명시적 실패를 검증하고, 검사와 rename 사이에 남는 외부 writer 경합 한계를 기록.

현재 계획의 미실행 항목은 완료된 것으로 보고하지 않습니다. 상세 결과는 [`TEST_RESULTS.md`](TEST_RESULTS.md)에 있습니다.
