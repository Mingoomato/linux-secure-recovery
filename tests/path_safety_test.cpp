#include "../src/path_safety.hpp"

#include <cstdio>
#include <string>

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::fprintf(stderr, "check failed at line %d: %s\n", __LINE__, #condition); \
        return 1; \
    } \
} while (0)

int main() {
    CHECK(sre::path::safe_relative_path("file"));
    CHECK(sre::path::safe_relative_path("nested/file.txt"));
    CHECK(sre::path::safe_relative_path("nested/한글.txt"));
    CHECK(sre::path::safe_relative_path(std::string(NAME_MAX, 'a')));
    CHECK(!sre::path::safe_relative_path(""));
    CHECK(!sre::path::safe_relative_path("/absolute"));
    CHECK(!sre::path::safe_relative_path("."));
    CHECK(!sre::path::safe_relative_path(".."));
    CHECK(!sre::path::safe_relative_path("../escape"));
    CHECK(!sre::path::safe_relative_path("a/../escape"));
    CHECK(!sre::path::safe_relative_path("./child"));
    CHECK(!sre::path::safe_relative_path("a//b"));
    CHECK(!sre::path::safe_relative_path("a/"));
    CHECK(!sre::path::safe_relative_path(std::string(NAME_MAX + 1U, 'x')));
    CHECK(!sre::path::safe_relative_path(std::string(PATH_MAX, 'x')));
    const std::string embedded_nul("safe\0suffix", 11U);
    CHECK(!sre::path::safe_relative_path(embedded_nul));

    CHECK(sre::path::safe_snapshot_id("snap-2026_10"));
    CHECK(!sre::path::safe_snapshot_id(""));
    CHECK(!sre::path::safe_snapshot_id("../escape"));
    CHECK(!sre::path::safe_snapshot_id(std::string(129U, 'a')));
    CHECK(!sre::path::safe_snapshot_id(std::string("id-\xff", 4U)));
    return 0;
}
