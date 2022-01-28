
cmake_minimum_required(VERSION 3.10)
project(Fble)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED True)
set(CMAKE_C_EXTENSIONS OFF)

add_subdirectory(fble/src)
add_subdirectory(tools)
add_subdirectory(prgms)
