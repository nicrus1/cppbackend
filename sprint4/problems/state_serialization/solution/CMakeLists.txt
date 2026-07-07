cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)

include(${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
conan_basic_setup(TARGETS)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# Collect all source files
set(GAME_SOURCES
    src/model.cpp
    src/game_state.cpp
    src/loot_manager.cpp
    src/loot_generator.cpp
    src/http_server.cpp
    src/logger.cpp
    src/json_loader.cpp
    src/htmldecode.cpp
    src/request_handler.cpp
    src/extra_data.cpp
    src/boost_json.cpp
)

add_library(game_model STATIC ${GAME_SOURCES})

# Include directories
target_include_directories(game_model PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/db
)

target_link_libraries(game_model PUBLIC CONAN_PKG::boost Threads::Threads)

# Main executable
add_executable(game_server
    src/main.cpp
)

target_include_directories(game_server PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/db
)

target_link_libraries(game_server game_model CONAN_PKG::boost Threads::Threads)

# Tests
add_executable(game_server_tests
    tests/state-serialization-tests.cpp
)

target_include_directories(game_server_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/db
)

target_link_libraries(game_server_tests CONAN_PKG::catch2 game_model)