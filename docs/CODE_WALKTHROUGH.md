# SRE 코드 워크스루

## 코드 읽기 순서

현재 구현은 작은 포트폴리오 범위에 맞춰 대부분의 동작을 [`src/sre.cpp`](../src/sre.cpp)에 둡니다. 다음 순서로 읽으면 저장소 경계에서 파일 복원까지의 흐름이 보입니다.

1. `main` — 인자와 명령 디스패치: `init`, `snapshot`, `list`, `show`, `verify`, `restore`, `prune`, `doctor`.
2. `canonical_dir`, `StoreLock`, `ensure_store_dirs` — 입력 루트와 저장소 구성, 명령 직렬화.
3. `src/path_safety.hpp`, `open_beneath`, `scan_tree`, `save_blob`, `sha256_fd`, `same_state` — 경로 문법, 안전한 열기, 재귀 순회, 콘텐츠 저장과 안정성 확인.
4. `open_db`, `manifest_document`, `write_manifest_atomic`, `manifest_matches_db` — DB 스키마와 매니페스트 계약.
5. `cmd_snapshot`, `cmd_show`, `cmd_verify` — 생성, 조회, 검증.
6. `ensure_parent`, `restore_one`, `cmd_restore` — 대상 디렉터리 FD 처리와 원자적인 파일 단위 복원.
7. `cmd_prune`, `cmd_doctor`, `audit` — 참조 보존, 잔여 상태 복구, 작업 로그.

## snapshot 제어 흐름

`cmd_snapshot`은 먼저 반복형 `--exclude-extension` 인자를 검증하고 루트 교차 여부와 저장소 잠금을 확인합니다. source root FD를 열고 `COPYING` 상태를 짧은 SQLite 트랜잭션으로 저장합니다. 이후 엔트리 트랜잭션 안에서 `scan_tree`가 항목을 순회합니다. 디렉터리는 `directory_entries`에 경로를 기록하고 빈 디렉터리도 manifest에 남깁니다. symlink, 특수 파일, 다른 device는 내용을 열지 않고 `excluded_entries`에 경로·유형·사유만 기록합니다. extension filter와 일치하는 정규 파일도 blob을 읽기 전에 제외 목록에 추가됩니다. 나머지 파일은 `open_beneath`로 열고 `save_blob`가 읽기와 해싱을 수행합니다. 파일의 `dev`, `ino`, `size`, `mtime`이 복사 전후 달라지면 snapshot은 실패하고, 안정성 확인 시점의 wall-clock nanoseconds가 `observed_at_ns`로 저장됩니다.

`cmd_init`은 저장소 경로의 부모를 먼저 canonicalize하고 prospective store와 source의 포함 관계를 검사합니다. 새 저장소가 source 아래라면 디렉터리를 만들기 전에 거부해 source 안에 초기화 부산물이 남지 않게 합니다. mkdir 이후 실제 경로도 다시 확인합니다. `safe_relative_path`는 별도 header의 bounded lexical check이고, 실제 파일 접근의 dirfd/openat2 제약을 대체하지 않습니다.

새 blob은 `temp/<snapshot>-<index>.part`에 `O_EXCL`로 작성됩니다. 파일 `fsync`와 최종 콘텐츠 주소 경로로의 `renameat2(RENAME_NOREPLACE)`, shard 디렉터리 `fsync`가 성공해야 DB 엔트리가 해당 해시를 참조합니다. 이미 존재하는 blob은 symlink를 따르지 않고 일반 파일, 크기, 해시, 안정 상태를 다시 확인합니다.

파일·디렉터리·제외 항목 트랜잭션이 완료되면 상태는 `VERIFIED`가 됩니다. `manifest_document`가 이 DB 상태에서 canonical JSON을 만들고, 무결성 해시는 해시 필드가 빠진 JSON payload의 SHA-256입니다. 빈 directory/exclusion 목록은 이전 `manifest.v1` 표현과의 호환성을 위해 생략하고 비어 있지 않은 목록만 각각 `directories`, `excluded_entries`로 담습니다. sidecar 파일을 임시 경로에 쓴 뒤 파일을 `fsync`, manifest 디렉터리 안에서 원자적으로 설치하고 디렉터리를 `fsync`합니다. 마지막 DB 트랜잭션이 상태를 `COMMITTED`로 바꿉니다. SQLite와 일반 파일시스템은 하나의 원자 트랜잭션이 아니므로 모든 crash interleaving은 아직 fault injection으로 입증되지 않았습니다.

## show와 verify

`cmd_show`는 snapshot ID를 제한된 문자 집합으로 확인하고, DB에서 `source_root`, `created_at`, `state`, `incomplete_count`, 파일 수, byte 합계, 디렉터리 수, 제외 수를 읽어 JSON으로 출력합니다. 존재하지 않는 ID와 `__store__`는 상세 조회 대상이 아닙니다.

`cmd_verify`는 커밋된 snapshot의 DB 엔트리를 읽고 sidecar의 bytes가 DB에서 다시 생성한 canonical JSON과 같은지 확인합니다. 이어 각 content-addressed blob을 `O_NOFOLLOW`로 열어 일반 파일 여부, 크기, 읽기 전후 stat 상태, SHA-256을 확인합니다. 불량 항목은 `bad`를 늘리고 exit 2를 반환합니다. 감사 이벤트 기록 자체가 실패하면 명령 결과도 실패할 수 있습니다.

## restore 흐름

`cmd_restore`는 sidecar가 DB와 맞는지 우선 검사합니다. dry-run은 dirfd 기준으로 부모 경로를 symlink-follow 없이 살피고 파일과 디렉터리 계획을 JSONL로 출력합니다. 없는 파일/디렉터리는 create, 기존 파일은 기본 preserve, `--overwrite` 파일은 replace, symlink/non-directory 부모와 overwrite 대상 directory는 blocked로 분류합니다. 요약은 삭제 0건과 target write 미실행을 기록하며 계획 레코드는 별도 Draft 2020-12 schema로 검증됩니다. 이 관찰은 시점이 고정된 예약이 아닙니다. 실제 복원은 `--confirm`을 요구하고, no-overwrite 설치는 `RENAME_NOREPLACE`로 충돌을 다시 확인하지만, overwrite 시점의 대상은 계획 후 달라질 수 있습니다.

`ensure_parent`는 root FD를 복제해 `mkdirat`와 `openat(O_DIRECTORY|O_NOFOLLOW)`로 대상 부모를 단계별로 엽니다. `restore_directory`는 빈 디렉터리를 포함한 디렉터리를 경로 순서로 만들며, 기존 directory는 유지하고 파일·symlink가 같은 이름을 차지하면 실패합니다. 디렉터리 권한·소유권·시간은 manifest에 포함하지 않습니다. `restore_one`은 blob을 열고 대상 디렉터리에 고유 임시 파일을 만듭니다. 내용을 복사하고 파일을 `fsync`, 다시 열어 크기와 SHA-256을 확인한 다음 mode·uid/gid·mtime 적용을 시도합니다. metadata 적용 실패는 복원 파일 자체를 실패로 취급하지 않고 `metadata_partial` 결과로 나타납니다. `renameat2`로 파일 하나를 설치하고 설치 이름을 `O_NOFOLLOW`로 다시 열어 inode·크기·hash를 확인한 뒤 부모 디렉터리를 `fsync`합니다. post-install 확인 실패는 이미 이름이 교체된 뒤 발생할 수 있어 오류 반환이 자동 rollback을 뜻하지 않습니다. 트리 전체가 all-or-nothing이라고 보장하지 않습니다.

## prune, doctor, audit

`cmd_prune`은 최신 `keep`개 committed snapshot을 남기고, 삭제 대상의 크기를 계산한 뒤 SQLite 트랜잭션으로 참조 행을 제거합니다. DB commit 이후 sidecar를 지우고 manifest 디렉터리를 `fsync`합니다. 남은 **committed** DB 행에서 참조 해시 집합을 만든 다음 참조가 없는 정규 파일 blob을 정리합니다. 실패 snapshot의 행은 live blob reference로 취급하지 않습니다. DB commit 후 파일 정리가 실패하면 일부 sidecar/고아 blob이 남을 수 있으며 `doctor`의 고아 manifest 정리와 후속 prune이 복구 경로입니다.

`cmd_doctor`는 stale `COPYING`/`VERIFIED` 상태를 `FAILED`로 전환합니다. SQLite의 `COMMITTED` ID 목록을 기준으로 manifest 디렉터리를 훑어 고아 `.json` 파일을 제거하고 디렉터리를 동기화합니다. 이어 `temp/`에서 `.part` 파일 또는 심볼릭 링크를 제거합니다. 테스트의 상태 삽입은 합성된 DB 상태이며 프로세스 강제 종료 증거가 아닙니다.

`audit`는 `O_APPEND|O_CREAT|O_NOFOLLOW`로 `audit/events.ndjson`을 열고 owner-owned regular file이며 hard link가 하나인지 확인한 뒤 0600 모드로 맞춥니다. 레코드에는 operation, snapshot ID, affected count, byte count, confirmed, observed time이 들어가며 append 후 `fsync`합니다. `byte_count`는 논리 바이트 합이며 prune에서 실제 디스크 reclaim 크기를 뜻하지 않습니다. 로컬 로그는 서명되지 않았고 같은 사용자는 수정할 수 있습니다. Audit 기록이 snapshot commit 이후 실패할 수 있으므로 명령 비정상 종료가 데이터 rollback을 뜻하지는 않습니다.

## 자원 수명과 디버깅 지점

`UniqueFd`는 FD를 소유하고 복사가 금지됩니다. `StoreLock`도 내부 FD를 소유해 RAII로 잠금을 풀고 닫습니다. 그 외 `rootfd`, `src`, `dst`, SQLite statement/connection은 함수 안에서 명시적으로 정리하므로 예외가 발생하는 모든 분기에서 소유권을 따라가야 합니다. OpenSSL EVP context 역시 오류 처리와 finalization을 확인합니다.

에러 분석 시 `errno`를 덮어쓰기 전에 저장하고, SQLite 오류는 해당 연결을 닫기 전에 복사하며, `close`, `fsync`, `renameat2` 결과를 구분해 기록하세요. 회귀 검증은 [`tests/smoke.sh`](../tests/smoke.sh)에서 수행합니다. 여기에는 sidecar/blob tamper, metadata, `show`, audit schema, prune 공유 blob, doctor 및 lock wait가 있습니다.

경로 문법 단위/퍼저 검증은 [`tests/path_safety_test.cpp`](../tests/path_safety_test.cpp), [`tests/path_fuzz.cpp`](../tests/path_fuzz.cpp), `SRE_ENABLE_FUZZING` CMake 옵션에서 확인합니다. 이 퍼저는 pure parser의 불변식을 검사하며 실제 syscall 경로나 전체 restore 경쟁을 퍼징하지 않습니다.
