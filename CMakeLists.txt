cmake_minimum_required(VERSION 3.24)  # FIND_PACKAGE_ARGS

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(TypeIt)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerWarnings)

option(TYPEIT_BUILD_TESTS "Build the test suite" ${PROJECT_IS_TOP_LEVEL})
include(Dependencies)

if(TYPEIT_BUILD_TESTS)
    add_subdirectory(tests)
endif()

add_subdirectory(src)
