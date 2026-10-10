cmake_minimum_required(VERSION 3.16)
project(linux_secure_recovery VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenSSL REQUIRED COMPONENTS Crypto)
find_package(SQLite3 REQUIRED)

add_executable(sre src/sre.cpp)
target_compile_options(sre PRIVATE -Wall -Wextra -Wpedantic -Werror)
target_link_libraries(sre PRIVATE OpenSSL::Crypto SQLite3::SQLite3)

include(CTest)
if(BUILD_TESTING)
    add_executable(path_safety_test tests/path_safety_test.cpp)
    target_compile_options(path_safety_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
    add_test(NAME path-safety-unit COMMAND path_safety_test)
    add_test(NAME recovery-smoke COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/smoke.sh)
    set_tests_properties(recovery-smoke PROPERTIES
        ENVIRONMENT "BIN_DIR=${CMAKE_CURRENT_BINARY_DIR}")
endif()

option(SRE_ENABLE_FUZZING "Build the Clang libFuzzer path parser target" OFF)
if(SRE_ENABLE_FUZZING)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "SRE_ENABLE_FUZZING requires Clang with libFuzzer support")
    endif()
    add_executable(path_fuzz tests/path_fuzz.cpp)
    target_compile_options(path_fuzz PRIVATE
        -O1 -g -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)
    target_link_options(path_fuzz PRIVATE
        -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)
    if(BUILD_TESTING)
        add_test(NAME path-parser-fuzz-smoke
            COMMAND /bin/sh ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_path_fuzzer_smoke.sh
                    $<TARGET_FILE:path_fuzz>
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/path_fuzz_corpus)
        set_tests_properties(path-parser-fuzz-smoke PROPERTIES TIMEOUT 60)
    endif()
endif()
