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

## 과제 3 — SRS의 `show` 조회를 확장

**목표:** `sre show --store STORE --snapshot ID`가 committed/failed 상태와 파일 수·총 크기를 안전하게 요약하도록 독립적으로 개선합니다.

**읽을 위치:** `cmd_show`, SQLite 쿼리 준비/바인딩/정리, `json_quote`, `tests/smoke.sh`.

누락된 ID, 내부 sentinel ID, 빈 snapshot, 오래된 FAILED snapshot을 포함한 테스트를 추가합니다. 출력은 한 줄 JSON으로 유지하고 문자열 값은 JSON 계약에 맞춰 인코딩합니다.

**완료 기준:** 정상 출력은 실제 DB 집계와 일치하고, 잘못된 ID·누락된 snapshot은 성공처럼 보이지 않습니다. SQLite statement와 연결은 모든 경로에서 정리됩니다.

## 시도 기록

선택한 과제, 구현한 코드 경로, 실행한 테스트, 실패 로그와 해결 이유를 기록하세요. automated build 통과는 개인 숙련도를 뜻하지 않습니다. 소유자 재현 상태는 직접 제출한 코드와 설명을 확인하기 전까지 **미평가**입니다.
