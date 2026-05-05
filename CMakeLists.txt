cmake_minimum_required(VERSION 3.14)
project(OpenterfaceCore LANGUAGES C VERSION 0.1.0)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ── Core library ─────────────────────────────────────────────────────────
add_library(keymod STATIC src/keymod.c)
target_include_directories(keymod PUBLIC include)

# ── CLI demo ─────────────────────────────────────────────────────────────
option(BUILD_CLI "Build the CLI demo tool" ON)
if(BUILD_CLI)
    add_executable(keymod_cli cli/keymod_cli.c)
    target_link_libraries(keymod_cli PRIVATE keymod)
endif()

# ── Tests ────────────────────────────────────────────────────────────────
option(BUILD_TESTS "Build the unit tests" ON)
if(BUILD_TESTS)
    add_executable(keymod_test tests/keymod_test.c)
    target_link_libraries(keymod_test PRIVATE keymod)
    enable_testing()
    add_test(NAME keymod_test COMMAND keymod_test)
endif()
