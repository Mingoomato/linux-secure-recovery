# SRE 엔지니어링 학습 가이드

## 프로젝트 의도와 현재 상태

Secure Recovery Engine(SRE)은 변경 전에 사용자가 명시적으로 만든 로컬 파일 스냅샷을 검증하고 복원하는 C++20 학습 프로젝트입니다. `openat2`, 파일 디스크립터 기반 경로 처리, SHA-256, SQLite 상태 관리, `fsync`와 원자적 파일 교체를 통해 Linux 시스템 프로그래밍의 보안 경계를 공부하도록 설계했습니다. 손상 후 만든 백업은 손상 전 데이터를 복구하지 못하며, 이 프로젝트는 실전 랜섬웨어 방어 제품이나 불변 백업이라고 주장하지 않습니다.

구현에는 `init`, `snapshot`, `list`, `show`, `verify`, `restore`, `prune`, `doctor`가 있습니다. 이전 revision에서는 GCC/Clang 빌드, sanitizer, 정적 분석, CTest와 10,000-run path fuzz smoke를 수행했습니다. 현재 revision은 directory manifest, 명시적 제외 항목, extension filter, persistent content-blob quota와 restore 직전 target identity 재확인을 추가했으며 요청에 따라 이 변경 뒤 기술 검증은 실행하지 않았습니다. 구현은 SRS-04 전체 완료가 아닙니다. 전원 손실·실제 강제 종료 매트릭스, 자원 고갈 fault injection, 1 GiB 성능 측정, quota 경계 행렬, ACL/xattr, 권한별 소유권 복원, 트리 전체 일관성은 입증되지 않았습니다. 자동화 검증은 소유자의 개인 숙련도 증거가 아닙니다.

## 하위 시스템이 필요한 이유

| 하위 시스템 | 해결하는 문제 | 핵심 위치 |
|---|---|---|
| 루트 검증 | 원본과 저장소의 중첩, symlink 루트를 차단 | `canonical_dir`, `cmd_init`, `cmd_snapshot` |
| 경로 문법 검증 | DB/CLI에서 들어온 상대 파일 경로와 snapshot ID의 traversal 및 잘못된 바이트를 차단 | `src/path_safety.hpp`, `load_entries` |
| 저장소 잠금 | SRE의 복원·검증과 prune/GC가 서로 간섭하지 않게 직렬화 | `StoreLock`, `flock(LOCK_EX)` |
| 경로 제한 | 공격자가 바꿀 수 있는 문자열 경로가 루트 밖으로 나가거나 symlink를 따라가지 않게 함 | `open_beneath`, `openat2` |
| 안정 파일 읽기와 blob 저장 | 복사 중 변한 파일을 정상 스냅샷으로 기록하지 않고 content hash와 파일별 관측 시각을 기록 | `save_blob`, `sha256_fd`, `same_state`, `Entry::observed_at_ns` |
| SQLite 메타데이터 | 경로·해시·파일 메타데이터와 스냅샷 상태를 저장 | `open_db`, `scan_tree`, `cmd_snapshot` |
| blob quota | 저장소별 persistent content-byte 상한을 적용하고 기존 store는 unlimited 기본값으로 유지 | `store_settings`, `configured_blob_quota`, `blob_store_usage`, `save_blob` |
| 디렉터리와 제외 항목 | 빈 디렉터리를 복원 가능하게 저장하고, 지원하지 않는 항목을 읽지 않은 채 이유와 함께 기록 | `directory_entries`, `excluded_entries`, `scan_tree` |
| JSON sidecar | DB와 별도로 스냅샷 정보를 내보내고 자체 무결성 해시로 손상 감지 | `manifest_document`, `write_manifest_atomic`, `manifest_matches_db` |
| 검증과 복원 | 저장 blob을 확인하고 임시 파일을 통해 파일 단위로 교체 | `cmd_verify`, `restore_one`, `cmd_restore` |
| 보존 정리와 복구 | 오래된 참조와 고아 파일을 정리하고 중단된 상태를 분리 | `cmd_prune`, `cmd_doctor` |
| 감사 기록 | 실행한 작업, 건수, 바이트 수, 확인 여부를 로컬에 기록 | `audit`, `operation-v1.schema.json` |

## 데이터·제어 흐름

### 스냅샷

1. `cmd_snapshot`은 source/store 루트를 검증하고 둘이 겹치지 않는지 확인한 후 전용 `flock`을 얻습니다.
2. 원본 루트를 디렉터리 FD로 열고, SQLite에 `COPYING` 행을 먼저 커밋합니다.
3. `scan_tree`는 루트 FD에서 디렉터리를 열거하고 디렉터리 경로를 기록해 빈 폴더도 보존합니다. symlink, 특수 파일, 다른 장치의 항목은 대상 내용을 열지 않고 유형·경로·제외 사유를 기록합니다.
4. `--exclude-extension`은 일반 파일을 blob으로 읽기 전에 basename 접미사를 ASCII 대소문자 구분 없이 검사합니다. 일치 파일은 manifest 제외 목록에 이유를 기록하고 blob은 만들지 않습니다.
5. 나머지 일반 파일은 `open_beneath`로 열고, `save_blob`가 복사 전후 `fstat` 상태를 비교하며 읽기와 SHA-256 계산을 동시에 수행합니다. 안정성 확인 직후 wall-clock `observed_at_ns`를 저장합니다. 이 시간은 파일 관측 시각이며 트리 전체의 공통 snapshot 시각이 아닙니다.
6. 설정된 quota가 있으면 `blob_store_usage`가 `blobs/sha256` 아래의 no-follow 정규 파일 크기를 합산합니다. dedup hit은 기존 blob의 유형·크기·hash·안정성을 확인해 사용량을 늘리지 않습니다. 새 고유 blob은 남은 byte budget을 넘으면 `SRE_STORE_QUOTA_EXCEEDED`로 거부합니다. quota는 blob content만 포함하며 임시 파일·DB/WAL·manifest·audit와 파일시스템 여유 공간을 제한하지 않습니다.
7. blob은 저장소 안의 고유 임시 파일에 기록되고 파일 `fsync`, 같은 파일시스템의 rename, shard 디렉터리 `fsync`를 거친 뒤 DB 행이 참조합니다.
8. DB 파일·디렉터리·제외 행을 기록한 트랜잭션이 끝난 뒤 versioned JSON sidecar를 원자적으로 설치하고 디렉터리를 동기화합니다. 마지막 DB 트랜잭션이 `VERIFIED`를 `COMMITTED`로 바꿉니다.
9. `doctor`는 남은 `COPYING`/`VERIFIED`를 `FAILED`로 표시하고, 커밋되지 않은 sidecar와 `.part` 파일을 정리합니다.

### 조회·검증·복원·정리

- `show`는 SQLite 스냅샷 행과 파일·디렉터리·제외 항목 집계를 JSON으로 보여 줍니다. `list`는 요약 행을 여러 개 출력합니다.
- `load_entries`는 SQLite TEXT의 NUL 종료 문자열만 믿지 않고 `sqlite3_column_bytes` 길이로 전체 경로 바이트를 복사한 뒤 helper로 검사합니다. helper는 borrowed `string_view`를 동기 호출 안에서만 사용하므로 수명 연장은 없습니다.
- `verify`는 DB 엔트리와 매니페스트 sidecar의 정합성을 비교하고 각 blob이 일반 파일인지, 크기와 SHA-256이 맞는지 확인합니다.
- `restore --dry-run`은 dirfd/no-follow 기준으로 target 부모와 엔트리를 확인하고 파일의 create/preserve/replace/blocked 및 디렉터리 create/preserve 동작을 JSONL로 출력합니다. 계획과 summary는 `restore-plan-v1` schema를 따르며 delete=0, writes_performed=false를 나타냅니다. 실제 복원은 `--confirm`, 기존 파일 교체는 `--overwrite`가 필요합니다.
- 디렉터리는 경로 순으로 안전하게 만들고, 파일은 `restore_one`이 대상 디렉터리 FD 아래에 임시 파일을 만듭니다. blob을 복사하고 staged 파일의 크기·해시를 검사한 뒤 mode/소유권/mtime을 가능한 범위에서 적용합니다. 설치 직전에 no-follow `fstatat`으로 기존 대상의 `(device,inode,type)`이 처음 관찰한 값과 같은지 확인하고, 달라졌으면 `ESTALE`로 설치를 중단합니다. 이어 `renameat2`로 파일 하나를 설치하고 설치 이름을 `O_NOFOLLOW`로 다시 열어 inode/size/hash를 재확인한 뒤 parent directory를 동기화합니다. 이 재확인은 변경을 발견하는 간격을 줄이지만 검사와 rename 사이 외부 writer 경합을 원자적으로 막지는 않습니다. 새 snapshot은 directory mode/owner/timestamp를 저장하고, 파일 복원 후 새로 생성한 디렉터리에 깊은 순서로 적용합니다. 기존 directory는 보존하고 이전 형식 snapshot은 경로만 유지합니다. 이 코드 변경은 미검증입니다.
- `prune`은 retention 밖의 DB 행을 트랜잭션으로 삭제하고 sidecar를 제거한 후 여전히 참조되지 않는 blob을 수집합니다. 같은 blob을 다른 스냅샷이 참조하면 남깁니다. `init --max-store-bytes N`은 final blob namespace의 persistent content byte 상한을 저장하며 `0`은 무제한입니다.

## 소유권, 수명, 동시성

- `Entry`, `std::string`, `std::vector`는 C++ 소유 객체입니다. SQLite 텍스트를 DB에 전달할 때 `SQLITE_TRANSIENT`로 복사하고, 쿼리 결과는 statement가 끝나기 전에 C++ 문자열로 복사해야 합니다.
- 파일·디렉터리 FD와 SQLite statement/connection은 C API 자원입니다. `UniqueFd`와 `StoreLock`은 RAII로 닫지만, 일부 경로는 여전히 `close`/`sqlite3_finalize`를 명시적으로 관리합니다. OpenSSL `EVP_MD_CTX`는 생성 후 모든 경로에서 해제해야 합니다.
- `StoreLock`의 FD는 명령 전체 동안 살아 있으며 소멸 시 잠금과 FD를 해제합니다. `flock`은 advisory이므로 잠금을 따르지 않는 프로세스는 저장소를 변경할 수 있습니다.
- 내부 worker thread는 없습니다. SRE 프로세스는 저장소별 배타 잠금으로 직렬 실행됩니다. 원본을 변경하는 외부 프로그램은 잠금 대상이 아닙니다.
- `scan_tree`는 파일별 관측 시점을 고정하지 않습니다. 각 파일은 안정 읽기인지 확인하지만 스냅샷 트리 전체가 하나의 시점에서 만들어졌다고 보장하지 않습니다.

## 보안 전제와 실패 방식

저장소와 상위 경로는 신뢰할 수 있고 현재 사용자 소유라는 전제가 있습니다. `openat2`가 지원되지 않으면 경로 제한을 완화하지 않고 실패합니다. JSON 경로 계약 때문에 유효하지 않은 UTF-8 파일 이름은 snapshot 전체를 실패시킵니다. 해시는 우발적 변조를 감지하지만 서명이나 작성자 인증이 아닙니다. root, 동일 사용자, 잠금을 무시하는 프로세스, 원본과 백업을 함께 파괴하는 디스크 고장에 대한 보호는 없습니다.

주요 실패는 `EINTR`, `ENOSPC`, `EIO`, 권한 오류, source 변경, blob/sidecar 손상, fsync/rename 실패, 복원 충돌입니다. 복원은 파일 단위로 진행하므로 일부 파일만 성공할 수 있습니다. mode와 mtime은 smoke fixture에서 확인했습니다. `fchown`은 시도하지만 권한이 부족하면 부분 메타데이터 결과가 됩니다. Audit 기록은 commit 뒤 실패할 수 있어 비정상 종료가 snapshot rollback을 뜻하지 않습니다. 실제 디스크·전원 장애에서 내구성은 검증하지 않았습니다.

## 대안 설계

- Btrfs/ZFS/LVM snapshot은 트리 전체의 일관성을 만들 수 있지만 파일시스템·권한에 묶이고 별도 저장소 보호를 자동으로 주지 않습니다.
- tar/rsync는 간단하고 익숙하지만 이 프로젝트의 DB 상태, 해시 주소 blob, sidecar 무결성 계약은 별도로 구현해야 합니다.
- 원격 불변 객체 저장소는 로컬 root/디스크 위험을 줄일 수 있지만 인증, 키 관리, 네트워크 재시도, retention 정책을 더해야 합니다.
- 전체 트리 임시 생성 후 교체는 더 강한 트리 단위 복원 경계를 만들 수 있지만 공간과 rename 제한이 늘어납니다.
- 여러 reader와 하나의 writer를 위한 공유 잠금은 병렬 읽기를 허용할 수 있습니다. 현재 배타 잠금은 단순하지만 처리량이 낮습니다.

## 독립 학습 진행

1. `docs/CODE_WALKTHROUGH.md`의 함수 순서대로 직접 코드를 따라갑니다.
2. `docs/LINUX_INTERNALS_EXPLAINED.md`에서 각 시스템 호출이 지키는 경계를 정리합니다.
3. `docs/DEBUGGING_EXERCISES.md`에서 한 과제를 선택해 별도 브랜치에서 구현하고 테스트 증거를 남깁니다. 과제 풀이 코드는 제공하지 않습니다.
4. `docs/TECHNICAL_INTERVIEW.md` 질문에 코드 위치와 근거를 들어 답합니다.

## 공식 참고 자료

- [`openat2(2)`](https://man7.org/linux/man-pages/man2/openat2.2.html), [`openat(2)`](https://man7.org/linux/man-pages/man2/openat.2.html), [`path_resolution(7)`](https://man7.org/linux/man-pages/man7/path_resolution.7.html)
- [`fsync(2)`](https://man7.org/linux/man-pages/man2/fsync.2.html), [`rename(2)`](https://man7.org/linux/man-pages/man2/rename.2.html), [`flock(2)`](https://man7.org/linux/man-pages/man2/flock.2.html), [`stat(2)`](https://man7.org/linux/man-pages/man2/stat.2.html)
- [SQLite WAL](https://www.sqlite.org/wal.html), [SQLite Atomic Commit](https://www.sqlite.org/atomiccommit.html), [SQLite foreign keys](https://www.sqlite.org/foreignkeys.html)
- [OpenSSL EVP digest API](https://docs.openssl.org/3.0/man3/EVP_DigestInit/)
- [LLVM libFuzzer](https://llvm.org/docs/LibFuzzer.html)
