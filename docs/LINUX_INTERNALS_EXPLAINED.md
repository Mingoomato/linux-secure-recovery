# SRE에서 사용하는 Linux 내부 인터페이스

## 디렉터리 FD와 `openat2`

일반 `open("문자열 경로")`는 현재 경로 구성요소가 바뀌는 경쟁 상황에서 의도하지 않은 위치를 가리킬 수 있습니다. SRE는 source root를 디렉터리 FD로 고정하고 상대 경로를 `openat2(2)`로 엽니다. `RESOLVE_BENEATH`는 루트 아래의 해석을 요구하고, `RESOLVE_NO_SYMLINKS`는 경로 중 symlink를 거부하며, `RESOLVE_NO_MAGICLINKS`는 procfs magic link를 거부합니다. 오래된 커널에서 약한 문자열 경로 호출로 대체하지 않고 실패하는 이유는 요구하는 보안 경계가 유지되지 않기 때문입니다.

`readdir`는 이름을 열거할 뿐 해당 파일을 안전하게 열었다는 뜻은 아닙니다. SRE는 `fstatat(..., AT_SYMLINK_NOFOLLOW)`로 종류를 확인한 다음 루트 FD에서 다시 상대 경로를 엽니다. 현재 `st_dev` 비교는 다른 device를 건너뛰는 제한된 정책이며 동일 device bind mount 전체를 식별하는 정책은 아닙니다.

디렉터리의 경우 `scan_tree`는 `fstatat`의 device/inode와 `openat2`로 연 FD의 `fstat` device/inode를 비교합니다. 이름이 검사와 열기 사이에 다른 디렉터리로 교체된 상황을 실패로 분류합니다. 지원하지 않는 symlink/특수 항목은 `AT_SYMLINK_NOFOLLOW`의 metadata만 읽고 link target이나 device/socket/FIFO 내용을 열지 않습니다. `--exclude-extension`은 일반 파일을 열기 전에 basename을 검사하므로 선택한 확장자의 내용은 blob에 들어가지 않습니다.

그 전에 `src/path_safety.hpp`의 `safe_relative_path(std::string_view)`가 길이, 선행 slash, NUL, 빈 구성요소, `.`/`..`, `NAME_MAX`를 검사합니다. 이 검사는 syscall 기반 containment보다 얕은 문법 검증입니다. `load_entries`가 SQLite TEXT를 복사할 때 `sqlite3_column_bytes`를 사용하므로 NUL 뒤의 바이트도 사라지지 않고 검사 대상에 포함됩니다. `cmd_init`도 source/store 경로 overlap을 새 store 생성 전에 검사해 거부 동작의 부작용을 줄입니다.

## 파일 안정성, 해시, 메모리

`open` 이후 FD는 열린 inode를 가리킵니다. `fstat`를 복사 전후 실행해 device, inode, 크기, 나노초 mtime을 비교하면 흔한 동시 변경을 감지합니다. 이는 원본 전체를 잠그지 않으며 모든 파일이 같은 시점에 존재했던 것처럼 보장하지도 않습니다.

파일 내용은 고정 크기 버퍼로 흘려 보내면서 OpenSSL EVP SHA-256 context를 갱신합니다. 메모리 사용량은 파일 전체 크기 대신 버퍼 크기와 일부 메타데이터 목록에 주로 비례합니다. SHA-256은 데이터가 예상 digest와 같은지 확인할 뿐, 생성자 신원·진본성·암호화를 제공하지 않습니다.

`observed_at_ns`는 안정성 비교가 끝난 직후 `system_clock`에서 읽은 Unix epoch nanoseconds입니다. 운영체제 wall clock은 조정될 수 있으므로 이 값은 순서 보장용 monotonic clock이나 트리 전체 snapshot timestamp가 아닙니다. 기존 DB에는 SQLite schema migration으로 컬럼을 추가하고 기본값 0을 사용해, 과거 manifest를 다시 생성할 때 새 필드를 임의로 붙이지 않습니다.

## 임시 파일, `fsync`, rename

새 blob은 전용 temp 디렉터리에 `O_CREAT|O_EXCL|O_NOFOLLOW`로 만듭니다. `write`는 짧게 쓰거나 `EINTR`로 실패할 수 있으므로 `write_all`이 진행 상태를 추적합니다. 파일에 `fsync`를 수행한 뒤 rename하고 상위 shard 디렉터리도 `fsync`합니다. 파일 `fsync`는 내용과 inode 메타데이터를 저장 장치 계층으로 밀어내고, 디렉터리 `fsync`는 새 이름의 디렉터리 엔트리를 내구성 있게 기록하는 데 필요합니다. 실제 보장은 파일시스템, 마운트 옵션, 장치 캐시와 커널에 따라 달라 실제 전원 장애 검증 없이는 확정할 수 없습니다.

복원은 대상과 같은 디렉터리에 임시 파일을 만들기 때문에 `renameat2`가 같은 파일시스템 안에서 하나의 이름 변경을 원자적으로 수행할 수 있습니다. `RENAME_NOREPLACE`는 덮어쓰기를 허용하지 않는 모드에서 마지막 경합을 방어합니다. `--overwrite`는 기존 대상 이름을 교체할 수 있습니다. 원자성은 파일 하나에만 해당하고 여러 파일을 하나의 트랜잭션으로 만들지 않습니다.

rename 이후에도 새 경로를 `openat(O_NOFOLLOW)`로 다시 열어 설치된 inode, 크기, SHA-256을 확인합니다. `fstat` 전후 안정성 확인은 해시 계산 중 같은 열린 inode의 변화를 찾고, `fstatat(..., AT_SYMLINK_NOFOLLOW)`는 검사 끝에 해당 이름이 여전히 같은 inode를 가리키는지 확인합니다. 이 검사 또는 directory `fsync`가 실패하면 이미 파일 이름 교체가 일어난 뒤일 수 있으므로 오류 결과가 자동 rollback을 뜻하지 않습니다.

`restore --dry-run`은 target root FD를 복제하고 각 기존 부모를 `openat(O_DIRECTORY|O_NOFOLLOW)`로 확인합니다. 마지막 이름은 `fstatat(..., AT_SYMLINK_NOFOLLOW)`로 조사하므로 기존 symlink의 목적지를 열지 않습니다. 디렉터리 부모가 없으면 필요한 나머지 부모 수를 계획에 기록하고, symlink/non-directory 부모와 `--overwrite` 대상 directory는 차단 항목으로 표시합니다. 이 검사는 target tree에 쓰지 않지만 작업 전후를 잠그지 않으므로 실제 `restore`까지 대상이 그대로라고 보장하지 않습니다. Plan JSONL은 경로 문자열과 상태 enum, 계획 크기, 충돌 action 및 errno를 보존하고 schema로 확인합니다.

빈 디렉터리도 `directories` 계획에 포함됩니다. directory 계획 함수는 `openat`와 `fstatat`만 사용하여 target을 바꾸지 않고, 실제 적용 경로의 `restore_directory`만 `mkdirat`로 생성합니다. 기존 directory는 보존하고 같은 이름의 symlink나 일반 파일은 실패시킵니다. 현재 directory mode, owner, timestamp는 저장하거나 적용하지 않으며 file metadata와 구분해야 합니다.

## SQLite 트랜잭션과 파일시스템

연결은 WAL과 `synchronous=FULL`을 설정하고 외래 키를 활성화합니다. Snapshot 상태는 `COPYING → VERIFIED → COMMITTED`로 진행합니다. SQLite transaction은 SQLite DB 안의 행을 원자적으로 다루지만 blob 파일과 JSON sidecar를 함께 묶는 분산 트랜잭션은 아닙니다. 코드의 순서와 `doctor` 복구 규칙은 상태를 해석하기 위한 것이며, crash/power-loss 매트릭스로 검증된 보장은 아닙니다.

`prune`은 먼저 DB에서 오래된 snapshot 참조를 commit한 뒤 sidecar를 지우고, 살아 있는 엔트리들이 참조하는 blob 목록을 만든 뒤 GC를 수행합니다. 중단 지점에 따라 orphan sidecar나 blob이 남을 수 있습니다. 먼저 파일을 삭제하고 DB transaction을 되돌리는 경우에는 반대로 DB가 가리키는 sidecar가 없어질 수 있으므로 순서가 중요합니다.

## advisory lock과 동시성

`flock(LOCK_EX)`는 같은 lock inode를 사용하는 SRE 명령을 직렬화합니다. 잠금은 advisory입니다. 외부 `sqlite3` 도구, root, 다른 프로그램이 잠금을 지키지 않으면 경합을 막지 않습니다. lock FD가 살아 있는 동안 잠금이 유지되고 `StoreLock` destructor가 종료 시 해제합니다. thread를 사용하지 않으며 현재 읽기와 쓰기 모두 같은 배타 잠금을 씁니다.

## 메타데이터 복원과 부분 성공

`fchown`은 uid/gid를 설정하고 `fchmod`는 권한 비트를 설정합니다. 소유권 변경은 일반 사용자에게 허용되지 않을 수 있으며, 소유권 변경은 setuid/setgid 비트에도 영향을 줄 수 있으므로 코드가 mode를 다시 적용합니다. `futimens`는 저장된 mtime을 설정하고 atime은 `UTIME_OMIT`로 유지합니다. 어느 하나가 실패하면 결과에 `metadata_partial`을 올립니다. 현재 smoke는 복원 파일의 UID/GID/mode/mtime이 source와 같은지 검사하지만, 다른 UID를 복원하는 권한 경계나 거부되는 `fchown`의 partial 경로는 시험하지 않았습니다.

## 자원 수명 점검

- `UniqueFd`와 `StoreLock`은 소유 FD를 destructor에서 닫습니다.
- `fdopendir`를 사용하면 전달한 FD를 DIR이 소유하므로 원본 FD를 닫지 않도록 복제합니다.
- SQLite statement는 `sqlite3_finalize`, DB 연결은 `sqlite3_close`로 끝내야 합니다.
- EVP context는 `EVP_MD_CTX_new/free` 수명이 맞아야 합니다.
- 임시 파일은 실패 경로에서 unlink해야 하며, 최종 rename 이후에는 해당 이름을 지우면 안 됩니다.

## 공식 문서

- [`openat2(2)`](https://man7.org/linux/man-pages/man2/openat2.2.html), [`openat(2)`](https://man7.org/linux/man-pages/man2/openat.2.html), [`path_resolution(7)`](https://man7.org/linux/man-pages/man7/path_resolution.7.html)
- [`fsync(2)`](https://man7.org/linux/man-pages/man2/fsync.2.html), [`rename(2)`](https://man7.org/linux/man-pages/man2/rename.2.html), [`flock(2)`](https://man7.org/linux/man-pages/man2/flock.2.html), [`stat(2)`](https://man7.org/linux/man-pages/man2/stat.2.html), [`utimensat(2)`](https://man7.org/linux/man-pages/man2/utimensat.2.html)
- [SQLite WAL](https://www.sqlite.org/wal.html), [SQLite Atomic Commit](https://www.sqlite.org/atomiccommit.html), [SQLite foreign keys](https://www.sqlite.org/foreignkeys.html)
- [OpenSSL EVP digest API](https://docs.openssl.org/3.0/man3/EVP_DigestInit/)
- [LLVM libFuzzer](https://llvm.org/docs/LibFuzzer.html)
