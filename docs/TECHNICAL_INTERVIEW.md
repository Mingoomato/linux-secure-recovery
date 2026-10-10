# SRE 기술 방어 질문

질문만 수록합니다. 답변은 제공하지 않습니다. 답변할 때 코드 위치, 시스템 호출 의미, 현재 검증 증거와 남은 한계를 구분하세요.

## 코드 읽기와 데이터 흐름

1. `scan_tree`에서 하나의 파일이 `entries` DB 행이 되기까지 함수 호출과 데이터 변화를 순서대로 설명하세요.
2. `open_beneath`의 `RESOLVE_BENEATH`, `RESOLVE_NO_SYMLINKS`, `RESOLVE_NO_MAGICLINKS`는 각각 어떤 경계를 구성합니까?
3. `same_state`가 비교하는 필드는 무엇이고 어떤 변경을 감지하지 못할 수 있습니까?
4. snapshot에서 blob 파일의 durability point와 manifest가 참조할 수 있는 시점을 구분하세요.
5. `COPYING → VERIFIED → COMMITTED` 전이를 설명하고 sidecar 설치가 어느 트랜잭션 사이에 있는지 말하세요.
6. `manifest_integrity_hash`는 어떤 bytes를 해시하며, 왜 서명과 동일하지 않습니까?
7. `show`와 `verify`의 역할 차이와 각 명령이 읽는 저장소 상태를 설명하세요.
8. 복원에서 staged file은 언제 해시되고, rename 이후 검증은 무엇을 확인합니까?

## 디버깅과 실패 추론

9. `save_blob`가 `EINTR`, `ENOSPC`, `EIO`, fsync 오류를 만났을 때 파일과 DB에 남을 수 있는 상태를 추적하세요.
10. SQLite commit 뒤 manifest sidecar 설치가 실패하면 무엇이 사용자에게 성공/실패로 보이고 `doctor`가 무엇을 정리합니까?
11. `prune`의 DB commit 직후 프로세스가 끝나면 어떤 sidecar나 blob이 남을 수 있습니까?
12. stale `VERIFIED` 행은 왜 자동으로 `COMMITTED`로 승격되지 않습니까?
13. smoke test의 합성 stale-state fixture가 실제 전원 손실 복구를 입증하지 못하는 이유는 무엇입니까?
14. `restore --dry-run`의 target plan이 point-in-time 관찰인 이유는 무엇이며, `--overwrite`를 실제 적용할 때 어떤 경쟁 조건이 남습니까?
15. 외부 프로세스가 원본을 복사 중 변경하면 어떤 파일은 안정적이어도 트리 snapshot이 혼합 시점이 될 수 있는 이유는 무엇입니까?

## 보안 추론

16. root 경로의 문자열 prefix 비교만으로 하위 경로를 판단하면 왜 부족합니까?
17. `fstatat(..., AT_SYMLINK_NOFOLLOW)` 뒤에 `openat2`를 다시 호출하는 이유는 무엇입니까?
18. 복원 대상 중간 디렉터리가 symlink라면 `ensure_parent`가 어떤 결과를 내야 합니까?
19. SHA-256 검증과 manifest 진본성은 어떤 차이가 있습니까?
20. 로컬 store가 root, 동일 사용자, 같은 디스크 장애에 취약한 이유를 위협 모델로 설명하세요.
21. `flock`을 무시하는 외부 프로세스는 어떤 동시성 위험을 만들 수 있습니까?
22. `fchown`, `fchmod`, `futimens` 실패가 부분 복원 결과로 처리되는 이유와 보안 영향을 설명하세요.

## 설계 선택과 검증

23. 파일 단위 atomic rename, 전체 트리 임시 생성, filesystem snapshot 방식의 보장과 비용을 비교하세요.
24. 현재 배타적 저장소 잠금을 공유 reader/exclusive writer 모델로 바꾸려면 어떤 경합과 GC 규칙을 새로 증명해야 합니까?
25. quota 지원을 추가할 때 snapshot 도중인 blob, 참조 중 blob, prune DB transaction의 순서를 어떻게 다뤄야 합니까?
26. 100개 crash injection 지점을 설계한다면 어떤 상태 경계와 기존 데이터 불변 조건을 검증합니까?
27. 다른 배포판과 파일시스템에서 `fsync`/rename 보장을 검증하기 위해 어떤 실험 기록이 필요합니까?
28. 현재 CTest, ASan/UBSan, Clang analyzer 결과가 각각 어떤 결함 종류를 놓칠 수 있습니까?

오너의 학습·재현 여부는 별도 추적합니다. 이 문서나 자동화된 테스트 통과만으로 개인 숙련도를 판정하지 않습니다.
