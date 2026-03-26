cmake_minimum_required(VERSION 3.20)
project(input_server)

set(CMAKE_CXX_STANDARD 23)

file(GLOB_RECURSE SOURCES src/*.cpp)

add_executable(input_server ${SOURCES})
target_include_directories(input_server PRIVATE src/)