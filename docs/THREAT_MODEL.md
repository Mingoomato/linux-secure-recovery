# SRE 위협 모델

## 보호 대상과 의도

SRE는 명시적으로 만든 사전 snapshot을 우발적인 파일 손상/삭제 뒤에 확인하고 복원하는 교육용 로컬 엔진입니다. 자동 pre-write 백업, 악성코드 격리, 실전 침해 방어를 제공하지 않습니다.

## 구현된 통제

- source와 store의 동일/중첩 경로를 거부합니다.
- symlink root와 symlink 파일을 따라가지 않으며 `openat2`의 beneath/no-symlink/no-magiclink 제약을 요구합니다. symlink·device·socket·FIFO와 다른 device의 항목은 내용을 열지 않고 manifest에 path/type/reason만 기록합니다.
- 빈 디렉터리 경로와 사용자가 지정한 extension exclusions를 manifest에 기록합니다. 제외 목록은 선택적으로 백업되지 않은 경로의 이름을 노출하므로 manifest 접근도 store owner로 제한합니다.
- 파일별 전후 stat 상태를 비교하고 content SHA-256을 확인합니다.
- blob과 manifest를 temp → fsync → rename 순서로 설치합니다.
- restore 쓰기에 `--confirm`, 기존 파일 교체에 별도 `--overwrite`를 요구합니다.
- target 아래의 한 파일씩 설치하고 source에 없는 경로를 삭제하지 않습니다.
- manifest/blob 손상은 verify와 restore에서 거부합니다.
- SRE 프로세스끼리 `flock`으로 직렬화하고 operation audit log를 owner-only 모드로 씁니다.

## 신뢰 가정과 제외 위험

Store와 상위 디렉터리는 현재 사용자 소유의 신뢰 경로라고 가정합니다. root나 같은 사용자 권한의 공격자는 blob/DB/log를 바꿀 수 있습니다. 잠금을 무시하는 외부 프로그램은 경합을 만들 수 있습니다. 원본과 백업이 같은 디스크면 해당 디스크 고장에 취약합니다. 감사 로그와 manifest는 서명되지 않습니다.

SRE는 ACL/xattr/hardlink identity, 트리 전체 시점 일관성, quota, 원격 저장, 암호화, root 공격자 방어를 제공하지 않습니다. UID/GID 복원은 권한에 따라 실패할 수 있습니다. Crash durability와 storage fault 처리 주장은 현재 시험하지 않은 범위입니다.
