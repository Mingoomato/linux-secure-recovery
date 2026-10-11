# Validation checkpoint — 2026-10-11

한국어: 최신 코드의 기본 빌드와 아래 한정된 기능 시험을 수행했습니다. 이 기록이 이전의 “현재 변경 미검증” 기록보다 최신이며, SRS 전체 완료나 소유자 숙련도를 뜻하지 않습니다.

English: This checkpoint covers the current source hashes in [validation-checkpoint.json](validation-checkpoint.json). It supersedes earlier unbuilt/unverified statements only for the checks explicitly listed here. It does not certify all SRS gates or owner mastery.

## Environment and commands

Linux 7.1.5+kali-amd64, x86_64; GCC 16.2.0; CMake Release; Python 3.14.7.

```sh
cmake -S . -B build-portfolio -DCMAKE_BUILD_TYPE=Release
cmake --build build-portfolio -j2
ctest --test-dir build-portfolio --output-on-failure
```

## Result and scope

3/3 CTest passed, 2.23 s.

Path safety, restore smoke, actual nested/empty-directory metadata restoration, persistent quota, dedup at quota, lowered quota, unlimited accounting and snapshot integrity after quota rejection.

## Remaining limits

Crash/power-loss, resource-fault, external-writer race and privilege/OS matrices remain pending.

## CI and source provenance

[Build and test workflow](../.github/workflows/ci.yml) runs GCC/Clang jobs on Ubuntu 24.04. The action is pinned to the [official checkout v4.2.2 commit](https://github.com/actions/checkout/releases/tag/v4.2.2). Kernel loading and privileged experiments are excluded from hosted runners. A workflow definition alone is not remote CI evidence; consult the actual run in GitHub Actions and the portfolio report.

[Source hashes and machine-readable scope](validation-checkpoint.json) identify the files exercised. Documentation and CI additions do not imply that pending runtime/fault matrices passed.
