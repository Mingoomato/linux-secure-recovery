# Linux Secure Recovery Engine

[:kr: 한국어](#한국어) · [:us: English](#english)

## 한국어

SRE는 일반 파일을 로컬 SHA-256 콘텐츠 주소 저장소에 스냅샷하고, 저장 blob을 검증한 뒤 명시적 확인을 받은 경우 파일 단위로 복원하는 학습용 엔진입니다. 목적은 `openat2` 경로 제한, 안정적인 파일 복사, 콘텐츠 중복 제거, SQLite manifest 상태, fsync와 원자적 rename을 학습하는 것입니다. 손상 이후 만든 스냅샷은 손상 전 바이트를 복구할 수 없습니다.

### 빌드 및 실행

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
mkdir -p "$HOME/recovery-lab" "$HOME/.local/share/sre-store" "$HOME/recovery-target"
./build/sre init --source "$HOME/recovery-lab" --store "$HOME/.local/share/sre-store"
./build/sre snapshot --source "$HOME/recovery-lab" --store "$HOME/.local/share/sre-store"
./build/sre verify --store "$HOME/.local/share/sre-store" --snapshot SNAPSHOT_ID
./build/sre restore --store "$HOME/.local/share/sre-store" --snapshot SNAPSHOT_ID --target "$HOME/recovery-target" --dry-run
```

복원 쓰기에는 `--confirm`이 필요합니다. 기존 파일은 기본적으로 건너뛰며 덮어쓰려면 `--overwrite`가 필요합니다. 대상의 다른 경로를 삭제하지 않습니다. C++20, OpenSSL, SQLite, CMake 및 `openat2`를 지원하는 Linux 커널이 필요합니다.

### 검증 및 한계

현재 Release 빌드와 로컬 스냅샷/검증/드라이런/복원 CTest 1/1이 통과했고 별도 ASan/UBSan 실행도 통과했습니다. 두 파일(역슬래시 포함 이름), 심볼릭 링크 제외, 저장 blob 해시, dry-run 비변경, 기존 파일 충돌 보존, 파일 복원, 합성 `COPYING` 행 복구와 임시 파일 정리, 프로세스 간 저장소 잠금을 확인합니다. 실제 전원 손실·프로세스 강제 종료 fault injection, blob 손상, 동시 원본 변경, ENOSPC, 대규모 성능은 검증하지 않았습니다.

SRE CLI 작업은 저장소의 비공개 `.operation.lock`에 배타적 `flock`을 얻어 직렬화합니다. `doctor`는 이전 실행의 `COPYING` 행을 `FAILED`/불완전 상태로 바꾸고 남은 `.part` 임시 파일을 정리합니다. 일관성은 파일별 안정 읽기이며 트리 전체의 단일 시점 스냅샷이 아닙니다. 로컬 저장소는 root, 잠금을 따르지 않는 프로세스, 동일 사용자, 같은 디스크 고장에 대비하지 않습니다. ACL/xattr, 소유권/mtime 복원, 원격 저장, 암호화, 전원 손실 수준의 crash durability 증거는 없습니다. 자세한 내용은 [`docs/CONSISTENCY_MODEL.md`](docs/CONSISTENCY_MODEL.md), [`docs/TEST_RESULTS.md`](docs/TEST_RESULTS.md)를 참고하세요.

## English

The SRE utility snapshots regular files into a local SHA-256 content-addressed store, records versioned metadata in SQLite, verifies stored blobs, and restores files through same-directory temporary files and atomic rename. It is intended for explicit pre-change lab snapshots; a snapshot made after damage cannot recover earlier bytes.

## Build

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --parallel

Dependencies: C++20 compiler, OpenSSL libcrypto, SQLite3, CMake, and a Linux kernel with openat2 support. The program fails closed when openat2 is unavailable.

## Example

    mkdir -p "$HOME/recovery-lab" "$HOME/.local/share/sre-store" "$HOME/recovery-target"
    ./build/sre init --source "$HOME/recovery-lab" --store "$HOME/.local/share/sre-store"
    ./build/sre snapshot --source "$HOME/recovery-lab" --store "$HOME/.local/share/sre-store"
    ./build/sre list --store "$HOME/.local/share/sre-store"
    ./build/sre verify --store "$HOME/.local/share/sre-store" --snapshot SNAPSHOT_ID
    ./build/sre restore --store "$HOME/.local/share/sre-store" --snapshot SNAPSHOT_ID --target "$HOME/recovery-target" --dry-run
    ./build/sre restore --store "$HOME/.local/share/sre-store" --snapshot SNAPSHOT_ID --target "$HOME/recovery-target" --confirm

Existing files are skipped unless --overwrite is explicit. The engine never deletes destination paths. Restore is per-file and reports partial failures; it does not claim all-or-nothing tree semantics. All CLI operations serialize through an owner-private advisory `flock`; `doctor` marks stale `COPYING` snapshots failed and removes abandoned `.part` files.

Run `ctest --test-dir build --output-on-failure` for the isolated snapshot, verify, dry-run, conflict, restore, synthetic stale-snapshot recovery, temp cleanup, and lock-wait smoke test. It uses temporary directories and requires a kernel with openat2 support.

## Limitations

The store and source must be disjoint, but this is a local store and does not resist a root attacker, a process that ignores advisory locks, or same-disk failure. Symlinks and non-regular files are excluded. Whole-tree point-in-time consistency, remote backups, encryption, ACLs, xattrs, ownership restoration, and real crash-injection evidence are not implemented. See `docs/TEST_RESULTS.md` for executed scope and remaining validation.
