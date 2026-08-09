# Every external dependency is declared here, once (ADR-007).
#
# FIND_PACKAGE_ARGS means an installed package is preferred and a download is
# the fallback, so a distribution build never reaches the network and a fresh
# clone never needs one to be installed. Every ref is a tag, never a branch:
# a floating ref makes a build unreproducible the day upstream pushes.

include(FetchContent)

set(TYPEIT_FTXUI_TAG v5.0.0 CACHE STRING "FTXUI version to build against")
set(TYPEIT_GOOGLETEST_TAG v1.16.0 CACHE STRING "GoogleTest version to build against")
set(TYPEIT_TOMLPLUSPLUS_TAG v3.4.0 CACHE STRING "toml++ version to build against")
set(TYPEIT_SQLITE_VERSION 3530400 CACHE STRING "SQLite amalgamation version to build against")

# Only relevant when FTXUI is built from source rather than found.
set(FTXUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
    GIT_TAG ${TYPEIT_FTXUI_TAG}
    GIT_SHALLOW TRUE
    FIND_PACKAGE_ARGS 5.0.0 CONFIG)

FetchContent_MakeAvailable(ftxui)

if(TYPEIT_BUILD_TESTS)
    # Windows: match the runtime library the rest of the build uses, or the
    # test binary fails to link with a mismatched CRT.
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG ${TYPEIT_GOOGLETEST_TAG}
        GIT_SHALLOW TRUE
        FIND_PACKAGE_ARGS 1.16 NAMES GTest CONFIG)

    FetchContent_MakeAvailable(googletest)
endif()

# SQLite ships an amalgamation rather than a CMake project, so the archive is
# hash-pinned and the target is declared here. A downloaded archive is verified
# by hash for the same reason a git dependency is pinned to a tag: a build that
# trusts whatever the server sends today is not reproducible tomorrow.
#
# find_package(SQLite3) is a CMake builtin module, so a distro or vcpkg SQLite
# wins and nothing is downloaded at all.
find_package(SQLite3 QUIET)
if(NOT TARGET SQLite::SQLite3)
    # The amalgamation is C, and this project declares only C++. Enabled here
    # rather than in project() so the common path — a distro or vcpkg SQLite —
    # does not pay for a compiler detection it never uses.
    enable_language(C)

    FetchContent_Declare(
        sqlite_amalgamation
        URL https://www.sqlite.org/2026/sqlite-amalgamation-${TYPEIT_SQLITE_VERSION}.zip
        URL_HASH SHA3_256=628a44cfe82c66aed1ccbbe85a562d2e33ebe64b3288981ed76285612227934e
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(sqlite_amalgamation)

    add_library(sqlite3_amalgamation STATIC "${sqlite_amalgamation_SOURCE_DIR}/sqlite3.c")
    target_include_directories(sqlite3_amalgamation SYSTEM PUBLIC "${sqlite_amalgamation_SOURCE_DIR}")
    # The features this project actually uses, and nothing else. Every option
    # off is code not compiled, not linked and not exposed.
    target_compile_definitions(
        sqlite3_amalgamation
        PRIVATE SQLITE_DQS=0
                SQLITE_THREADSAFE=1
                SQLITE_DEFAULT_MEMSTATUS=0
                SQLITE_OMIT_DEPRECATED
                SQLITE_OMIT_LOAD_EXTENSION
                SQLITE_OMIT_SHARED_CACHE
                SQLITE_ENABLE_FTS5
                # `floor()` and friends have been opt-in since SQLite 3.35, and
                # the daily-totals query needs one. Without this the history
                # screen fails with `no such function: FLOOR` on every build
                # that does not find a system SQLite — which is every Windows
                # build, and which nobody saw because the Windows job never got
                # past compiling.
                SQLITE_ENABLE_MATH_FUNCTIONS)
    if(UNIX)
        find_package(Threads REQUIRED)
        target_link_libraries(sqlite3_amalgamation PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
    endif()
    add_library(SQLite::SQLite3 ALIAS sqlite3_amalgamation)
endif()

FetchContent_Declare(
    tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG ${TYPEIT_TOMLPLUSPLUS_TAG}
    GIT_SHALLOW TRUE
    FIND_PACKAGE_ARGS 3.4 CONFIG NAMES tomlplusplus)

FetchContent_MakeAvailable(tomlplusplus)
