# Every external dependency is declared here, once (ADR-007).
#
# FIND_PACKAGE_ARGS means an installed package is preferred and a download is
# the fallback, so a distribution build never reaches the network and a fresh
# clone never needs one to be installed. Every ref is a tag, never a branch:
# a floating ref makes a build unreproducible the day upstream pushes.

include(FetchContent)

set(TYPEIT_FTXUI_TAG v5.0.0 CACHE STRING "FTXUI version to build against")
set(TYPEIT_GOOGLETEST_TAG v1.16.0 CACHE STRING "GoogleTest version to build against")

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
