# Linux Secure Recovery Engine

[:kr: 한국어](#한국어) · [:us: English](#english)

## 한국어

### 목적

SRE는 사용자가 변경 전에 만든 로컬 파일 snapshot을 보관하고, 저장된 blob을 검증한 후 승인받아 복원하는 학습용 Linux 프로젝트입니다. 경로 제한, 안정 파일 읽기, SHA-256 콘텐츠 주소 저장, SQLite 상태 관리, 매니페스트 무결성, 원자적 파일 단위 복원을 학습합니다. 손상 후 만든 snapshot은 손상 전 데이터를 되살릴 수 없습니다. 실전 랜섬웨어 방어 또는 불변 백업 제품을 주장하지 않습니다.

### 기능

- 명령: `init`, `snapshot`, `list`, `show`, `verify`, `restore`, `prune`, `doctor`
- `restore --dry-run`은 항목별 생성·기존 대상 보존·교체·차단 계획과 요약을 JSON Lines로 출력합니다. dry-run은 target을 쓰거나 삭제하지 않습니다. 실제 쓰기에는 `--confirm`, 기존 파일 교체에는 추가로 `--overwrite`가 필요합니다. 계획은 현재 시점의 관찰이므로 실제 복원에서는 경합·권한·저장소 오류가 별도로 발생할 수 있습니다.
- 일반 파일만 처리하며 symlink, 특수 파일, 다른 device 항목은 제외합니다. 파일 크기 상한은 1 GiB이고 JSON 경로 계약에 맞지 않는 invalid UTF-8 파일 이름은 snapshot 실패로 처리합니다.
- mode, uid/gid, mtime을 저장합니다. 복원 시 mode와 mtime을 적용하며, uid/gid 적용 권한이 없으면 부분 메타데이터 상태를 보고합니다.
- 작업 이벤트를 hard link를 거부하는 로컬 NDJSON 감사 파일에 기록하고 `operation.v1` 스키마로 계약을 정의합니다. Snapshot 데이터가 commit된 뒤 감사 기록이 실패할 수 있으므로 오류 종료가 자동 rollback을 뜻하지 않습니다.

### 빌드와 실행

필요 환경은 C++20, CMake, OpenSSL libcrypto, SQLite3, `openat2`를 지원하는 Linux 커널입니다. CTest 스모크 실행에는 Python 3와 `python3 -m pip install -r requirements-test.txt`로 설치하는 `jsonschema`가 필요합니다.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
python3 -m pip install -r requirements-test.txt
ctest --test-dir build --output-on-failure

mkdir -p "$HOME/recovery-lab" "$HOME/.local/share/sre-store" "$HOME/recovery-target"
./build/sre init --source "$HOME/recovery-lab" --store "$HOME/.local/share/sre-store"
./build/sre snapshot --source "$HOME/recovery-lab" --store "$HOME/.local/share/sre-store"
./build/sre list --store "$HOME/.local/share/sre-store"
./build/sre show --store "$HOME/.local/share/sre-store" --snapshot SNAPSHOT_ID
./build/sre verify --store "$HOME/.local/share/sre-store" --snapshot SNAPSHOT_ID
./build/sre restore --store "$HOME/.local/share/sre-store" --snapshot SNAPSHOT_ID --target "$HOME/recovery-target" --dry-run
./build/sre restore --store "$HOME/.local/share/sre-store" --snapshot SNAPSHOT_ID --target "$HOME/recovery-target" --confirm
```

### 검증과 현재 한계

GCC Release, Clang Release, Clang 정적 분석, ASan/UBSan Debug의 로컬 빌드와 CTest가 통과했습니다. 스모크 검증은 snapshot 상세 조회, blob/sidecar 손상 검출과 복원 거부, schema 검증을 포함한 dry-run 충돌 계획, 실제 파일 내용 및 mode/mtime 복원, 감사 JSON Schema·권한·건수·바이트 수, prune과 공유 blob 보존, 합성 중단 상태의 `doctor`, 저장소 잠금 대기를 확인합니다. 자세한 기록은 [`docs/TEST_RESULTS.md`](docs/TEST_RESULTS.md)와 [`docs/REQUIREMENTS_TRACEABILITY.md`](docs/REQUIREMENTS_TRACEABILITY.md)에 있습니다.

실제 프로세스/전원 손실 crash matrix, ENOSPC/EIO/EACCES fault injection, 원본 동시 변경, 1 GiB 성능 측정과 복수 배포판 검증은 수행하지 않았습니다. quota, ACL/xattr, 트리 전체 일관성, 원격/암호화 백업은 구현되지 않았습니다. 저장소는 root나 같은 사용자의 변조, 잠금을 따르지 않는 프로그램, 같은 디스크 고장에 대한 보호를 제공하지 않습니다. 복원은 파일별로 이루어져 일부만 성공할 수 있습니다.

## English

### Purpose

SRE is an educational Linux project for storing user-created local snapshots made before a change, checking their content blobs, and restoring files after explicit approval. It demonstrates constrained path resolution, stable file reads, SHA-256 content addressing, SQLite snapshot state, manifest integrity, and atomic per-file restore. A snapshot made after damage cannot recover earlier bytes. SRE is not a production ransomware defense or immutable-backup product.

### Features

- Commands: `init`, `snapshot`, `list`, `show`, `verify`, `restore`, `prune`, and `doctor`.
- `restore --dry-run` emits a per-entry create/preserve/replace/blocked plan and summary as JSON Lines. It does not write or delete target files. Writes require `--confirm`; replacing an existing file also requires `--overwrite`. The plan is a point-in-time observation; races, permissions, or storage failures can still affect a later restore.
- Only regular files are processed. Symlinks, special files, entries on another device, files over 1 GiB, and invalid UTF-8 path names are rejected or excluded.
- Mode, uid/gid, and mtime are stored. Restore applies mode and mtime; unavailable uid/gid privileges are reported as partial metadata restoration.
- Local NDJSON operation audit records follow the `operation.v1` schema and reject hard-linked log files. Audit writing may fail after snapshot data is committed, so a command error does not imply an automatic rollback.

### Build and run

Requirements: a C++20 compiler, CMake, OpenSSL libcrypto, SQLite3, and a Linux kernel that supports `openat2`. The CTest smoke suite also requires Python 3 and `jsonschema` from `requirements-test.txt`.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
python3 -m pip install -r requirements-test.txt
ctest --test-dir build --output-on-failure

mkdir -p "$HOME/recovery-lab" "$HOME/.local/share/sre-store" "$HOME/recovery-target"
./build/sre init --source "$HOME/recovery-lab" --store "$HOME/.local/share/sre-store"
./build/sre snapshot --source "$HOME/recovery-lab" --store "$HOME/.local/share/sre-store"
./build/sre list --store "$HOME/.local/share/sre-store"
./build/sre show --store "$HOME/.local/share/sre-store" --snapshot SNAPSHOT_ID
./build/sre verify --store "$HOME/.local/share/sre-store" --snapshot SNAPSHOT_ID
./build/sre restore --store "$HOME/.local/share/sre-store" --snapshot SNAPSHOT_ID --target "$HOME/recovery-target" --dry-run
./build/sre restore --store "$HOME/.local/share/sre-store" --snapshot SNAPSHOT_ID --target "$HOME/recovery-target" --confirm
```

### Verification and limitations

GCC Release, Clang Release, Clang static analysis, and ASan/UBSan Debug builds and local CTest runs passed. The smoke suite checks snapshot details, corrupt blob/sidecar detection and restore refusal, schema-validated dry-run conflict plans, restored bytes and mode/mtime, audit schema/permissions/counts/bytes, prune with shared-blob retention, synthetic interrupted-state recovery, and lock waiting. See [`docs/TEST_RESULTS.md`](docs/TEST_RESULTS.md) and [`docs/REQUIREMENTS_TRACEABILITY.md`](docs/REQUIREMENTS_TRACEABILITY.md).

Real process/power-loss crash matrices, ENOSPC/EIO/EACCES fault injection, concurrent source mutation, a 1 GiB benchmark, and multi-distribution verification have not been run. Quota, ACL/xattr, whole-tree consistency, remote storage, and encryption are not implemented. The local store does not protect against root or same-user tampering, programs that ignore its advisory lock, or failure of the same disk. Restore is per file and may partially complete.
