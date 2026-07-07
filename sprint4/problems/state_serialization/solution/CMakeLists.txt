cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)

include(${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
conan_basic_setup(TARGETS)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# Создаем директорию для исходников
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/src/db)

add_library(game_model STATIC
    src/geom.h
    src/model_serialization.h
    src/model.h
    src/model.cpp
    src/tagged.h
    src/game.h
    src/app_listener.h
    src/logger.h
    src/sdk.h
    src/extra_data.h
    src/db/record_manager.h
)

target_link_libraries(game_model PUBLIC CONAN_PKG::boost Threads::Threads)

add_executable(game_server
    src/main.cpp
    src/serializing_listener.h
    src/api_handler.h
    src/request_handler.h
    src/request_handler.cpp
    src/http_server.h
    src/http_server.cpp
    src/command_line.h
    src/boost_json.cpp
)

target_link_libraries(game_server game_model CONAN_PKG::boost Threads::Threads)

add_executable(game_server_tests
    tests/state-serialization-tests.cpp
)

target_link_libraries(game_server_tests CONAN_PKG::catch2 game_model)