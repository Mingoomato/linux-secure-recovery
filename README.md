# Linux Secure Recovery Engine

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

Existing files are skipped unless --overwrite is explicit. The engine never deletes destination paths. Restore is per-file and reports partial failures; it does not claim all-or-nothing tree semantics.

Run `ctest --test-dir build --output-on-failure` for the isolated snapshot, verify, dry-run, conflict, and restore smoke test. It uses temporary directories and requires a kernel with openat2 support.

## Limitations

The store and source must be disjoint, but this is a local store and does not resist a root attacker or same-disk failure. Symlinks and non-regular files are excluded. Whole-tree point-in-time consistency, remote backups, encryption, ACLs, xattrs, ownership restoration, and crash-injection evidence are not implemented. See `docs/TEST_RESULTS.md` for executed scope and remaining validation.
