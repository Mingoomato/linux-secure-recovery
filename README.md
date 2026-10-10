# Linux Secure Recovery Engine

[:kr: 한국어](#한국어) · [:us: English](#english)

## 한국어

### 목적

SRE는 사용자가 변경 전에 만든 로컬 파일 snapshot을 보관하고, 저장된 blob을 검증한 후 승인받아 복원하는 학습용 Linux 프로젝트입니다. 경로 제한, 안정 파일 읽기, SHA-256 콘텐츠 주소 저장, SQLite 상태 관리, 매니페스트 무결성, 원자적 파일 단위 복원을 학습합니다. 손상 후 만든 snapshot은 손상 전 데이터를 되살릴 수 없습니다. 실전 랜섬웨어 방어 또는 불변 백업 제품을 주장하지 않습니다.

### 기능

- 명령: `init`, `snapshot`, `list`, `show`, `verify`, `restore`, `prune`, `doctor`
- Snapshot은 일반 파일, 디렉터리 트리와 빈 디렉터리를 기록합니다. symlink, 장치, socket, FIFO, 다른 device의 항목은 파일 내용을 따라가지 않고 경로·유형·제외 사유만 기록합니다. 반복 가능한 `--exclude-extension .tmp` 옵션은 basename 끝부분을 대소문자 구분 없이 비교해 파일을 제외하고 manifest에 이유를 남깁니다. 파일 크기 상한은 1 GiB이고 JSON 경로 계약에 맞지 않는 invalid UTF-8 파일 이름은 snapshot 실패로 처리합니다.
- `restore --dry-run`은 파일의 생성·기존 대상 보존·교체·차단과 디렉터리 생성·보존 계획을 JSON Lines로 출력합니다. dry-run은 target을 쓰거나 삭제하지 않습니다. 실제 쓰기에는 `--confirm`, 기존 파일 교체에는 추가로 `--overwrite`가 필요합니다. 계획은 현재 시점의 관찰이므로 실제 복원에서는 경합·권한·저장소 오류가 별도로 발생할 수 있습니다.
- 일반 파일마다 mode, uid/gid, mtime_ns, 관측 시각을 저장합니다. 복원 시 mode와 mtime을 적용하며, uid/gid 적용 권한이 없으면 부분 메타데이터 상태를 보고합니다. 디렉터리는 경로와 빈 디렉터리 복원 정보만 저장합니다.
- 파일 복원은 설치 전에 staged bytes를 확인하고, 원자적 rename 뒤 installed path의 inode·크기·SHA-256을 다시 확인합니다. 사후 확인이나 디렉터리 동기화 실패는 이미 설치된 파일을 자동으로 되돌리지 않습니다.
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
./build/sre snapshot --source "$HOME/recovery-lab" --store "$HOME/.local/share/sre-store" --exclude-extension .tmp --exclude-extension .cache
./build/sre list --store "$HOME/.local/share/sre-store"
./build/sre show --store "$HOME/.local/share/sre-store" --snapshot SNAPSHOT_ID
./build/sre verify --store "$HOME/.local/share/sre-store" --snapshot SNAPSHOT_ID
./build/sre restore --store "$HOME/.local/share/sre-store" --snapshot SNAPSHOT_ID --target "$HOME/recovery-target" --dry-run
./build/sre restore --store "$HOME/.local/share/sre-store" --snapshot SNAPSHOT_ID --target "$HOME/recovery-target" --confirm
```

### 검증과 현재 한계

이전 구현 revision에서는 GCC/Clang 빌드, 정적 분석, sanitizer, fuzz smoke와 CTest를 수행했습니다. 현재 revision에는 디렉터리/제외 항목 manifest, 확장자 필터, 파일별 `observed_at_ns`와 DB migration 변경이 추가됐으며, 요청에 따라 이 변경 뒤에는 빌드·테스트·정적 분석을 실행하지 않았습니다. 따라서 현재 변경 사항의 기술 검증 상태는 미확인입니다. 이전 증거와 적용 범위는 [`docs/TEST_RESULTS.md`](docs/TEST_RESULTS.md)에 분리해 기록합니다.

사용자별 일일 실행 예시는 [`docs/systemd/sre-snapshot.service`](docs/systemd/sre-snapshot.service)와 [`docs/systemd/sre-snapshot.timer`](docs/systemd/sre-snapshot.timer)에 있습니다. `%h` 경로와 실행 파일 경로를 환경에 맞게 수정한 뒤 `systemctl --user enable --now sre-snapshot.timer`로 활성화합니다.

실제 프로세스/전원 손실 crash matrix, ENOSPC/EIO/EACCES fault injection, 원본 동시 변경, 1 GiB 성능 측정과 복수 배포판 검증은 수행하지 않았습니다. quota, ACL/xattr, 트리 전체 일관성, 원격/암호화 백업은 구현되지 않았습니다. 저장소는 root나 같은 사용자의 변조, 잠금을 따르지 않는 프로그램, 같은 디스크 고장에 대한 보호를 제공하지 않습니다. 복원은 파일별로 이루어져 일부만 성공할 수 있습니다.

### 학습 패키지

한국어 학습 자료: [`docs/ENGINEERING_STUDY_GUIDE.md`](docs/ENGINEERING_STUDY_GUIDE.md), [`docs/CODE_WALKTHROUGH.md`](docs/CODE_WALKTHROUGH.md), [`docs/LINUX_INTERNALS_EXPLAINED.md`](docs/LINUX_INTERNALS_EXPLAINED.md), [`docs/DEBUGGING_EXERCISES.md`](docs/DEBUGGING_EXERCISES.md), [`docs/TECHNICAL_INTERVIEW.md`](docs/TECHNICAL_INTERVIEW.md). 구현 연습 3개는 완성 코드를 공개하지 않습니다. 저장소 검증과 소유자의 학습·재현 상태는 별도 추적하며, 직접 증거가 없어 현재 숙련도는 미평가입니다.

## English

### Purpose

SRE is an educational Linux project for storing user-created local snapshots made before a change, checking their content blobs, and restoring files after explicit approval. It demonstrates constrained path resolution, stable file reads, SHA-256 content addressing, SQLite snapshot state, manifest integrity, and atomic per-file restore. A snapshot made after damage cannot recover earlier bytes. SRE is not a production ransomware defense or immutable-backup product.

### Features

- Commands: `init`, `snapshot`, `list`, `show`, `verify`, `restore`, `prune`, and `doctor`.
- Snapshots record regular files, the directory tree, and empty directories. Symlinks, devices, sockets, FIFOs, and entries on another device are recorded by path, type, and exclusion reason without following or backing up their contents. Repeatable `--exclude-extension .tmp` filters compare case-insensitive basename suffixes and record excluded files in the manifest. The per-file limit is 1 GiB; invalid UTF-8 names fail the snapshot because they cannot be represented by this JSON path contract.
- `restore --dry-run` emits JSON Lines plans for file create/preserve/replace/block actions and directory create/preserve actions. It does not write or delete target files. Writes require `--confirm`; replacing existing files also requires `--overwrite`.
- Regular files store mode, uid/gid, mtime_ns, and observation time. Restore applies mode and mtime; unavailable uid/gid privileges are reported as partial metadata restoration. Directories store paths for empty-directory restoration, without directory metadata.
- File restore checks staged bytes and reopens the installed path after atomic rename to check inode, size, and SHA-256. A post-install or directory-sync error does not automatically roll back the installed name.
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
./build/sre snapshot --source "$HOME/recovery-lab" --store "$HOME/.local/share/sre-store" --exclude-extension .tmp --exclude-extension .cache
./build/sre list --store "$HOME/.local/share/sre-store"
./build/sre show --store "$HOME/.local/share/sre-store" --snapshot SNAPSHOT_ID
./build/sre verify --store "$HOME/.local/share/sre-store" --snapshot SNAPSHOT_ID
./build/sre restore --store "$HOME/.local/share/sre-store" --snapshot SNAPSHOT_ID --target "$HOME/recovery-target" --dry-run
./build/sre restore --store "$HOME/.local/share/sre-store" --snapshot SNAPSHOT_ID --target "$HOME/recovery-target" --confirm
```

### Verification and limitations

Earlier revisions passed GCC/Clang builds, static analysis, sanitizer, fuzz smoke, and local CTest. This revision adds directory/exclusion manifest records, extension filters, per-file `observed_at_ns`, and a database migration; per the user's instruction, no build, test, or static analysis was run after those edits. The current changes are therefore technically unverified. Earlier evidence and its scope are separated in [`docs/TEST_RESULTS.md`](docs/TEST_RESULTS.md).

A per-user daily timer example is in [`docs/systemd/sre-snapshot.service`](docs/systemd/sre-snapshot.service) and [`docs/systemd/sre-snapshot.timer`](docs/systemd/sre-snapshot.timer). Adjust the `%h` paths and binary path, then enable it with `systemctl --user enable --now sre-snapshot.timer`.

Real process/power-loss crash matrices, ENOSPC/EIO/EACCES fault injection, concurrent source mutation, a 1 GiB benchmark, and multi-distribution verification have not been run. Quota, ACL/xattr, whole-tree consistency, remote storage, and encryption are not implemented. The local store does not protect against root or same-user tampering, programs that ignore its advisory lock, or failure of the same disk. Restore is per file and may partially complete.

### Study package

The Korean study files are [`docs/ENGINEERING_STUDY_GUIDE.md`](docs/ENGINEERING_STUDY_GUIDE.md), [`docs/CODE_WALKTHROUGH.md`](docs/CODE_WALKTHROUGH.md), [`docs/LINUX_INTERNALS_EXPLAINED.md`](docs/LINUX_INTERNALS_EXPLAINED.md), [`docs/DEBUGGING_EXERCISES.md`](docs/DEBUGGING_EXERCISES.md), and [`docs/TECHNICAL_INTERVIEW.md`](docs/TECHNICAL_INTERVIEW.md). The three coding exercises intentionally omit complete solutions. Repository verification and owner study/reproduction status are tracked separately; owner mastery is unevaluated without direct evidence.
