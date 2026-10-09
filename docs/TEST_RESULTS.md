# Test results

Status: build and local recovery smoke test passed on the current Linux host, which supports openat2.

- Release configure/build: passed with C++20, OpenSSL Crypto, and SQLite3.
- `ctest --test-dir build --output-on-failure`: 1/1 passed.
- Separate AddressSanitizer/UndefinedBehaviorSanitizer debug build and the same smoke test: passed.
- The smoke test snapshots two regular files (including a backslash in one path), excludes a symlink, verifies the stored blobs, confirms dry-run creates no target file, preserves an existing conflict, and restores the original bytes into a clean target.

Not run: corrupt-blob tests, crash/fault injection, concurrent mutation, no-space handling, overwrite validation, extended sanitizer/static-analysis cases, or large-tree performance. The result covers per-file restore only; it does not establish whole-tree atomicity or crash durability.
