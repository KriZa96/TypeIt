cmake_minimum_required(VERSION 3.24)  # FIND_PACKAGE_ARGS

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(TypeIt)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerWarnings)
include(Sanitizers)
include(StaticAnalysis)

option(TYPEIT_BUILD_TESTS "Build the test suite" ${PROJECT_IS_TOP_LEVEL})
include(Dependencies)

# src first: it defines the typeit_legacy target the test binary links.
add_subdirectory(src)

# enable_testing() belongs at the top level, or ctest run from the build
# root -- which is what `ctest --preset` does -- finds nothing.
if(TYPEIT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
