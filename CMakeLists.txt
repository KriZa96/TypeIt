cmake_minimum_required(VERSION 3.15)

execute_process(
        COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
)

project(TypeIt)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(src)
add_subdirectory(tests)