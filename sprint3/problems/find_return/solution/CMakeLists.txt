cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Threads first
find_package(Threads REQUIRED)

include(${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
conan_basic_setup(TARGETS)

# Основной исполняемый файл сервера
add_executable(game_server
    src/main.cpp
    src/model.cpp
    src/json_loader.cpp
    src/request_handler.cpp
    src/game_state.cpp
    src/loot_manager.cpp
    src/loot_generator.cpp
    src/http_server.cpp
    src/logger.cpp
    src/htmldecode.cpp
    src/boost_json.cpp
)

target_include_directories(game_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(game_server PRIVATE 
    CONAN_PKG::boost
    Threads::Threads
)

# Тесты
add_executable(game_server_tests
    src/model.cpp
    src/game_state.cpp
    src/loot_manager.cpp
    src/loot_generator.cpp
    tests/tests.cpp
    tests/loot_tests.cpp
    tests/loot_generator_tests.cpp
    tests/collision_tests.cpp
)

target_include_directories(game_server_tests PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
)

target_link_libraries(game_server_tests PRIVATE 
    CONAN_PKG::catch2
    CONAN_PKG::boost
    Threads::Threads
)

set_target_properties(game_server game_server_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Включение тестирования
enable_testing()
add_test(NAME game_server_tests COMMAND game_server_tests)