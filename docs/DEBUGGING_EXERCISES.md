# 디버깅 및 독립 구현 과제

아래 과제는 완성 풀이를 제공하지 않습니다. 하나를 선택해 임시 브랜치와 `/tmp` 기반 테스트 저장소에서 직접 구현하세요. 실제 사용자 데이터로 복원 시험을 하지 마세요.

## 과제 1 — 대상 경로 symlink 경계 회귀 테스트

**목표:** 복원 대상 안의 symlink 부모 디렉터리가 대상 루트 밖 쓰기로 이어지지 않음을 증명합니다.

**읽을 위치:** `ensure_parent`, `restore_one`, `src/path_safety.hpp`, `tests/smoke.sh`.

중첩된 target 경로 구성요소를 외부 임시 디렉터리를 가리키는 symlink로 만들고 confirmed restore를 수행합니다. 반환값, 진단, 외부 디렉터리와 target 파일을 검사하고 통합 회귀 테스트를 추가합니다.

**완료 기준:** target root 바깥에 파일이 생성/교체되지 않고 실패가 관측됩니다. 기존 정상 중첩 경로 복원은 계속 통과합니다.

## 과제 2 — audit 경로의 symlink 공격 시험

**목표:** `audit/events.ndjson`이 심볼릭 링크 또는 외부 파일과 hard link를 공유할 때 외부 파일을 변경하지 않음을 증명합니다.

**읽을 위치:** `audit`, `UniqueFd`, `cmd_snapshot`, `contracts/operation-v1.schema.json`, `tests/smoke.sh`.

별도 임시 store에서 audit 경로를 외부 sentinel을 가리키는 symlink 및 hard link로 각각 준비합니다. snapshot 명령 결과, DB 상태, 외부 파일 bytes, 남은 blob/manifest 상태를 기록합니다. 실패가 snapshot commit 뒤에 발생할 수 있는지도 추적합니다.

**완료 기준:** sentinel은 바뀌지 않습니다. 결과는 committed 데이터와 audit 기록 실패를 구분할 수 있어야 하며, 테스트가 임시 디렉터리 밖을 건드리지 않습니다.

## 과제 3 — 디렉터리 메타데이터 보존 설계

**목표:** 현재 path-only directory manifest를 mode, uid/gid, mtime을 포함하는 계약으로 확장하고 복원 결과의 partial metadata 상태를 다룹니다.

**읽을 위치:** `DirectoryEntry`, `directory_entries`, `manifest_document`, `restore_directory`, `contracts/manifest-v1.schema.json`, `tests/smoke.sh`.

하위 디렉터리, 빈 디렉터리, 이미 존재하는 target directory, 쓰기 권한이 부족한 target을 포함하는 fixture를 구성하세요. 자식 파일 설치 뒤 디렉터리 시간을 적용하는 순서, 소유권 변경이 거부되는 경우, umask 영향을 설명하고 검증 가능한 결과 계약을 설계하세요.

**완료 기준:** manifest schema와 DB sidecar 정합성이 유지되고, 허용된 메타데이터는 정확히 복원되며, 거부된 metadata 적용은 명시적 partial 결과가 됩니다. 기존 파일 보호와 target 밖 경로 불변 조건을 회귀 시험으로 보존합니다.

## 시도 기록

선택한 과제, 구현한 코드 경로, 실행한 테스트, 실패 로그와 해결 이유를 기록하세요. automated build 통과는 개인 숙련도를 뜻하지 않습니다. 소유자 재현 상태는 직접 제출한 코드와 설명을 확인하기 전까지 **미평가**입니다.
