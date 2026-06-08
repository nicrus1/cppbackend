cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)

include(${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
conan_basic_setup(TARGETS)

# Основное приложение
add_executable(game_server
    src/main.cpp
    src/model.cpp
    src/loot_generator.cpp
    src/extra_data.cpp
)

# Тестовое приложение
add_executable(game_server_tests
    tests/catch_test_main.cpp
    tests/model-tests.cpp
    tests/loot_generator_tests.cpp
    src/model.cpp
    src/loot_generator.cpp
)

target_include_directories(game_server_tests PRIVATE src)
target_link_libraries(game_server_tests PRIVATE CONAN_PKG::catch2 Threads::Threads)