# Portfolio review / 포트폴리오 검토 안내

## 문제와 제작 의도

변경 전에 저장한 스냅샷으로 파일을 검증하며 복구하기. 이 저장소는 Linux 시스템 호출의 결과와 실패를 관측 가능한 계약으로 연결하는 교육용 구현입니다.

## 핵심 설계

SQLite manifest와 콘텐츠 주소형 blob을 분리합니다. 파일 임시 쓰기·fsync·rename·부모 fsync 순서로 게시하고 manifest 상태기계와 store lock으로 복구와 GC를 조율합니다. 새 디렉터리는 파일 복원 뒤 깊은 순서로 메타데이터를 적용합니다.

## 코드 검토 경로

1. [`src/sre.cpp`](../src/sre.cpp)의 CLI 진입점에서 자원 획득과 오류 반환을 찾습니다.
2. [코드 해설](CODE_WALKTHROUGH.md)에서 핵심 함수와 소유권을 따라갑니다.
3. [최신 검증 기록](VALIDATION_CHECKPOINT.md)과 [요구사항 추적표](REQUIREMENTS_TRACEABILITY.md)를 비교합니다.
4. [기술 질문](TECHNICAL_INTERVIEW.md)과 [정답 없는 실습](DEBUGGING_EXERCISES.md)으로 직접 설명·재현합니다.

검토 질문: 파일 하나의 atomic rename이 트리 전체 transaction이 아닌 이유는? quota에서 dedup blob은 어떻게 세는가? audit 실패가 이미 게시된 복원을 되돌리는가?

## English

**Purpose:** Restore a pre-change snapshot with explicit integrity checks.

SQLite manifests and content-addressed blobs have separate roles. Temporary writes, fsync, rename and parent fsync publish files; a manifest state machine and store lock coordinate recovery and GC. Newly created directories receive metadata deepest first after file restoration.

Start with the linked source entry point, follow `CODE_WALKTHROUGH.md`, and compare the dated checkpoint with requirement traceability. Review questions: Why is per-file atomic rename not a whole-tree transaction? How are deduplicated blobs counted against quota? Does an audit failure roll back an already published restore?

The five study documents cover subsystem purpose, data/control flow, Linux interfaces, memory/resource lifetime, concurrency, security assumptions, failures and alternatives. Three reproduction exercises remain unsolved. Repository checks and the owner's demonstrated competence are tracked separately; neither AI assistance nor automated test completion establishes personal mastery.
