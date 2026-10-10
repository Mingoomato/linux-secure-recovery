# SRE 일관성 및 내구성 모델

- 각 일반 파일은 복사 전후의 `dev`, `ino`, `size`, `mtime`이 동일한지 검사합니다. 이는 파일 단위 안정성 검사이며 전체 트리의 한 시점 snapshot이 아닙니다.
- blob bytes는 temp file에 기록되고 file `fsync`, 같은 filesystem rename, shard directory `fsync`를 거친 뒤 DB가 참조합니다.
- SQLite WAL/FULL transaction은 DB를 관리합니다. sidecar와 blob을 SQLite와 함께 묶는 원자 트랜잭션은 없습니다.
- manifest sidecar는 DB에서 재생성한 canonical JSON과 비교합니다. checksum은 변조 탐지용이며 서명이나 작성자 인증이 아닙니다.
- `doctor`는 `COPYING`/`VERIFIED`를 `FAILED`로 바꾸고, DB committed 목록과 맞지 않는 `.json` sidecar와 `temp/*.part`를 정리합니다. committed snapshot의 sidecar가 빠진 경우 복구 재생성하지 않으며 `verify`가 손상을 보고합니다.
- `prune`은 DB 참조를 먼저 commit하고 committed snapshot이 더 이상 참조하지 않는 sidecar/blob을 그 뒤에 삭제합니다. 실패 snapshot의 entry는 live blob으로 간주하지 않습니다. 프로세스 종료로 고아 파일이 남을 수 있으며 다음 `doctor`/`prune`에서 정리할 수 있습니다.
- 모든 SRE 명령은 하나의 저장소 잠금을 사용합니다. `flock`은 advisory이고 외부 쓰기 프로세스를 막지 않습니다.
- `restore`는 한 파일의 rename 단위 원자성만 목표로 합니다. 여러 파일의 all-or-nothing 복원은 제공하지 않습니다.
- Smoke test는 합성 DB 상태와 저장소 파일을 사용합니다. 실제 프로세스 crash, 커널 장애, 전원 손실, 장치 write cache 동작을 입증하지 않습니다.
