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
    add_test(NAME recovery-smoke COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/smoke.sh)
endif()
