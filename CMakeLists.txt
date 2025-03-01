cmake_minimum_required(VERSION 3.15)
project(TypeIt)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(VCPKG_DIR "${CMAKE_SOURCE_DIR}/external/vcpkg")
set(CMAKE_TOOLCHAIN_FILE "${VCPKG_DIR}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")

add_subdirectory(src)
add_subdirectory(tests)
