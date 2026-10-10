# SRE 기술 검증 결과

기준일: 2026-10-10 · 대상: 현재 Linux 작업 호스트 · SRS 전체 완료 판정이 아니라 실행한 게이트의 기록입니다.

## 상태 분리

| 추적 항목 | 상태 | 근거 |
|---|---|---|
| 구현 | PARTIAL | CLI와 주요 저장·검증·복원 흐름 구현. quota, 트리 전체 일관성, crash fault harness 등은 미구현 |
| 기술 검증 | 이전 revision 로컬 게이트 통과; 현재 변경 revision은 미검증 | directory/exclusion manifest 및 extension filter 추가 후 빌드·테스트·정적 분석을 실행하지 않음 |
| 오너 학습/재현 | 미평가 | 독립 과제 수행이나 코드 설명 증거를 아직 받지 않음 |

## 현재 revision의 구현 변경 후 상태

2026-10-10에 directory path manifest, 지원하지 않는 항목의 명시적 제외 기록, 반복형 extension filter, 파일별 `observed_at_ns` 및 legacy DB migration, directory restore 계획, per-user systemd timer 예시를 추가했습니다. 사용자가 검증을 중단하고 구현을 계속하라고 지시했으므로 이 변경 뒤에는 빌드, CTest, schema validation, analyzer, `git diff --check`를 실행하지 않았습니다. 아래에 적은 통과 결과는 변경 전 revision의 역사적 증거이며 현재 파일의 검증 결과로 해석하면 안 됩니다.

## 이전 revision에서 실행한 검증

- GCC Release CMake 빌드: 통과.
- GCC 16.2 Release 빌드/CTest: 2/2 통과 (`path-safety-unit`, `recovery-smoke`).
- Clang 21.1.8 Release 빌드/CTest: 2/2 통과.
- Clang Debug AddressSanitizer + UndefinedBehaviorSanitizer 빌드/CTest: 2/2 통과.
- 별도 Clang libFuzzer + ASan/UBSan 빌드/CTest: 3/3 통과. path parser fuzz smoke에서 10,000 runs 수행.
- `clang++ --analyze -std=c++20 -Wall -Wextra -Wpedantic -Werror src/sre.cpp`: 진단 없음.
- `git diff --check`: 통과.
- `manifest.v1` 및 `operation.v1` Draft 2020-12 Schema 검증: 스모크에서 Python `jsonschema`로 schema와 레코드 검증.

스모크에서 실제 확인한 항목:

- symlink source root 거부, 3개 정규 파일 snapshot, 파일 메타데이터와 `show` 집계.
- source/store 동일 경로, 상위/하위 중첩을 거부하며 초기화 전 하위 경로에 디렉터리나 DB를 만들지 않는 동작.
- relative path와 snapshot ID parser의 절대경로, traversal, 빈/점 구성요소, 경계 길이, NUL, ASCII 허용 문자 단위 검사.
- 유효한 UTF-8 Unicode 파일 이름 복원 및 잘못된 UTF-8 이름의 snapshot 거부.
- 매니페스트 및 blob 변조 시 검증 실패, 손상된 snapshot의 restore 거부, 기존 대상 보존.
- `restore-plan-v1` Draft 2020-12 schema와 JSONL 레코드 검증. 빈 target 생성 계획, 기존 regular file/symlink/directory 보존, `--overwrite`의 regular file/symlink 교체 및 directory 차단을 확인. 두 dry-run 모두 target에 쓰거나 삭제하지 않았고, 차단 계획은 exit 2로 알립니다.
- 기본 충돌 보존, 실제 파일 복원, mode와 nanosecond mtime 보존.
- `--overwrite`에 `--confirm`이 빠지면 exit 2, 기존 sentinel 보존; manifest 밖 unrelated target 파일도 보존.
- restored UID/GID/mode/mtime이 source metadata와 일치하는 smoke fixture 검사. 이 검사는 다른 UID 권한 경계나 강제 partial 복원을 입증하지 않음.
- audit NDJSON 각 이벤트 계약, 0600 파일 모드, snapshot/restore/prune 건수와 byte count.
- hard-linked audit target 거부 및 외부 sentinel 불변 확인. 이 경우 snapshot DB 상태는 이미 `COMMITTED`일 수 있어 로그 실패와 데이터 commit을 구분합니다.
- prune 이후 다른 snapshot이 참조하는 공유 blob의 유지.
- 실패 snapshot만 참조하는 orphan blob의 prune 회수.
- 합성 `COPYING`/`VERIFIED` 행, 고아 매니페스트 및 `.part` 정리, `doctor`의 반복 가능 동작.
- 두 번째 프로세스가 저장소 `flock`을 기다리는 동작.

테스트 스크립트는 `BIN_DIR`을 CTest 빌드 디렉터리로 설정합니다. 따라서 GCC, Clang, sanitizer 구성은 각각 자신이 빌드한 바이너리를 실행합니다.

## 미실행 또는 증거가 부족한 게이트

- 프로세스 강제 종료와 전원/커널/장치 장애를 포함한 crash matrix; 현재 `doctor` 검증은 합성 DB/파일 상태입니다.
- ENOSPC, EIO, EACCES 및 fsync/rename 실패 주입과 기존 committed snapshot 불변성.
- 실제 동시 원본 변경 경합과 snapshot unstable 검출 테스트.
- SRE-NFR-01의 1,000개/1 GiB 데이터, 처리량/RSS/CPU/중복률 benchmark.
- 2개 이상 배포판 및 파일시스템/마운트 옵션 조합.
- quota 지원과 quota/GC 경쟁 검증.
- 타 UID 및 거부되는 `fchown` 권한 경계, ACL/xattr 보존.
- 계획 이후 overwrite race, 파일 설치 후 재해시, cross-filesystem/특수 파일시스템 제한 테스트.
- 10,000-run smoke를 넘는 장시간 fuzzing 캠페인 및 SRS가 요구하는 100개 fault point.

이 범위 때문에 SRS-04 DoD는 충족으로 표시하지 않습니다. CTest, sanitizer, analyzer는 이 문서에 적힌 테스트 범위의 근거이며 사용자 숙련도를 증명하지 않습니다.
