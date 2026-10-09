# Test results

Status: build and local recovery smoke test passed on the current Linux host, which supports openat2.

- Release configure/build: passed with C++20, OpenSSL Crypto, and SQLite3.
- `ctest --test-dir build --output-on-failure`: 1/1 passed.
- Separate AddressSanitizer/UndefinedBehaviorSanitizer debug build and the same smoke test: 1/1 passed.
- The smoke test snapshots two regular files (including a backslash in one path), excludes a symlink, verifies stored blobs, confirms dry-run creates no target file, preserves an existing conflict, restores bytes into a clean target, seeds a stale `COPYING` row and `.part` artifacts for `doctor`, and checks a second process waits on the store lock.

Not run: corrupt-blob tests, real process/power-loss crash injection, concurrent source mutation, no-space handling, overwrite validation, extended static-analysis cases, or large-tree performance. The synthetic stale-row fixture does not establish crash durability. The result covers per-file restore only; it does not establish whole-tree atomicity.
