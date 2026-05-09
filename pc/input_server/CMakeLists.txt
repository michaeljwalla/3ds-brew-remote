cmake_minimum_required(VERSION 3.20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
project(input_server)

cmake_policy(SET CMP0167 NEW)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Debug)

find_package(Boost REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBEVDEV REQUIRED libevdev)

set(WEBSOCKETPP_INCLUDE_DIR /usr/local/include/websocketpp)

file(GLOB_RECURSE SOURCES src/*.cpp)

add_executable(input_server ${SOURCES})

target_include_directories(input_server PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${WEBSOCKETPP_INCLUDE_DIR}
    ${Boost_INCLUDE_DIRS}
    ${LIBEVDEV_INCLUDE_DIRS}
)
target_include_directories(input_server PUBLIC 
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(input_server PRIVATE
    pthread
    ${Boost_LIBRARIES}
    ${LIBEVDEV_LIBRARIES}
)
target_compile_options(input_server PRIVATE ${LIBEVDEV_CFLAGS_OTHER})