cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)

include(${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
conan_basic_setup(TARGETS)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_library(game_model STATIC
    src/geom.h
    src/model_serialization.h
    src/model.h
    src/model.cpp
    src/tagged.h
    src/game.h
    src/app_listener.h
)

target_link_libraries(game_model PUBLIC CONAN_PKG::boost Threads::Threads)

add_executable(game_server
    src/main.cpp
    src/serializing_listener.h
)

# Устанавливаем путь для исполняемого файла
set_target_properties(game_server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

target_link_libraries(game_server game_model CONAN_PKG::boost Threads::Threads)

add_executable(game_server_tests
    tests/state-serialization-tests.cpp
)

target_link_libraries(game_server_tests CONAN_PKG::catch2 game_model)