# SRE 아키텍처

1. CLI는 source/store/target root를 검증하고 각 저장소 작업에서 owner-private `.operation.lock`에 배타 `flock`을 얻습니다.
2. Snapshot traversal은 source directory FD를 기준으로 `readdir`, `fstatat(AT_SYMLINK_NOFOLLOW)`, `openat2(RESOLVE_BENEATH|RESOLVE_NO_SYMLINKS|RESOLVE_NO_MAGICLINKS)`를 사용합니다. 디렉터리 경로는 별도 테이블에 저장하고, symlink·특수·다른 device 및 사용자가 지정한 확장자 제외는 내용 접근 없이 사유와 함께 기록합니다.
3. 정규 파일은 확장자 필터와 크기 상한을 확인하고 고정 크기 버퍼로 읽습니다. 전후 `fstat`가 다르면 해당 snapshot은 실패합니다. 같은 bytes의 SHA-256은 content-addressed blob 이름이며, 안정성 검사 직후 per-file `observed_at_ns`를 기록합니다.
4. Blob은 temp 파일에 기록하고 file `fsync`, 같은 store filesystem 안에서 rename, shard directory `fsync` 후 DB entry가 참조합니다.
5. SQLite는 WAL, `synchronous=FULL`, foreign key를 설정합니다. snapshot row는 `COPYING`, `VERIFIED`, `COMMITTED` 상태를 거칩니다. DB와 파일시스템은 별도 commit domain입니다.
6. `manifest.v1` sidecar는 DB의 파일·디렉터리·제외 항목에서 canonical JSON으로 생성되고 SHA-256 integrity field를 포함합니다. signature/authentication을 제공하지 않습니다.
7. Restore dry-run은 target dirfd를 따라 파일·디렉터리 충돌 계획을 JSONL로 출력하며 target을 변경하지 않습니다. 승인된 적용은 디렉터리를 먼저 만들고 파일별 temp write/hash check/metadata attempt/`renameat2`/parent directory `fsync`로 처리합니다. 트리 전체 atomicity는 주장하지 않습니다.
8. Prune은 보존 밖 DB 참조를 삭제한 뒤 sidecar 및 committed snapshot이 참조하지 않는 blob을 정리합니다. Doctor는 stale state, orphan sidecar, `.part` 항목을 보수적으로 회수합니다.

이전 revision의 로컬 CTest/sanitizer/analyzer 결과는 남아 있지만 현재 directory/exclusion/extension 변경 후에는 검증을 실행하지 않았습니다. crash durability와 quota/benchmark 요구사항도 아직 충족되지 않았습니다. [`CONSISTENCY_MODEL.md`](CONSISTENCY_MODEL.md), [`THREAT_MODEL.md`](THREAT_MODEL.md), [`TEST_RESULTS.md`](TEST_RESULTS.md)를 참조하세요.
