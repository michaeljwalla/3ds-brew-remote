cmake_policy(SET CMP0167 NEW)
find_package(Boost REQUIRED)

cmake_minimum_required(VERSION 3.20)
project(input_server)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


# Find websocketpp headers (header-only library)
find_path(WEBSOCKETPP_INCLUDE_DIR websocketpp/config/asio_no_tls.hpp)

if(NOT WEBSOCKETPP_INCLUDE_DIR)
    message(FATAL_ERROR "websocketpp headers not found. Install libwebsocketpp-dev or add websocketpp to your project.")
endif()

file(GLOB_RECURSE SOURCES src/*.cpp)

add_executable(input_server ${SOURCES})

target_include_directories(input_server PRIVATE
    src/
    ${WEBSOCKETPP_INCLUDE_DIR}
)

target_link_libraries(input_server PRIVATE
    pthread
)