cmake_minimum_required(VERSION 3.20)
project(untitled)

set(CMAKE_CXX_STANDARD 20)

add_executable(untitled DialsortVSRadixsort.cpp)
target_link_libraries(untitled psapi)