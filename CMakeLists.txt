cmake_minimum_required(VERSION 3.24)  # FIND_PACKAGE_ARGS

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(
    TypeIt
    VERSION 2.0.0
    LANGUAGES CXX)

# The version is declared once, here, and every consumer reads the generated
# header (VERSIONING section 6).
set(TYPEIT_VERSION_PRERELEASE "alpha.1" CACHE STRING "SemVer pre-release identifier, or empty")

if(TYPEIT_VERSION_PRERELEASE STREQUAL "")
    set(TYPEIT_VERSION_FULL "${PROJECT_VERSION}")
else()
    set(TYPEIT_VERSION_FULL "${PROJECT_VERSION}-${TYPEIT_VERSION_PRERELEASE}")
endif()

# Diagnostic only, and empty in a tarball with no history.
set(TYPEIT_GIT_DESCRIBE "")
find_package(Git QUIET)
if(Git_FOUND AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" describe --tags --always --dirty
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        OUTPUT_VARIABLE TYPEIT_GIT_DESCRIBE
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
endif()

configure_file(cmake/Version.h.in "${PROJECT_BINARY_DIR}/generated/typeit/core/Version.h" @ONLY)

add_library(typeit_version INTERFACE)
target_include_directories(typeit_version INTERFACE "${PROJECT_BINARY_DIR}/generated")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerWarnings)
include(Coverage)
include(EmbedSchema)
include(Sanitizers)
include(StaticAnalysis)

option(TYPEIT_BUILD_TESTS "Build the test suite" ${PROJECT_IS_TOP_LEVEL})
include(Dependencies)

# enable_testing() belongs at the top level, or ctest run from the build
# root -- which is what `ctest --preset` does -- finds nothing. It comes before
# the subdirectories because apps/typeit registers tests of its own, and
# add_test() before enable_testing() is silently dropped.
if(TYPEIT_BUILD_TESTS)
    enable_testing()
endif()

# The rebuild. src/ below is the 1.0.0 tree, deleted whole at the Phase 4
# cutover (TI-097).
add_subdirectory(libs/core)
add_subdirectory(libs/app)
add_subdirectory(libs/cli)
add_subdirectory(libs/infra)
add_subdirectory(libs/tui)

add_subdirectory(apps/typeit)

# src first: it defines the typeit_legacy target the test binary links.
add_subdirectory(src)

if(TYPEIT_BUILD_TESTS)
    add_subdirectory(tests)
endif()
