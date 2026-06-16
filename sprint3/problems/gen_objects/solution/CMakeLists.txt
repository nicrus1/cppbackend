cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Подключаем Conan файлы, если они существуют
if(EXISTS ${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
    include(${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
    conan_basic_setup(TARGETS)
endif()

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

# Подключаем Catch2
if(TARGET CONAN_PKG::catch2)
    target_link_libraries(game_server_tests PRIVATE CONAN_PKG::catch2 Threads::Threads)
elseif(TARGET Catch2::Catch2)
    target_link_libraries(game_server_tests PRIVATE Catch2::Catch2 Threads::Threads)
else()
    # Пробуем найти Catch2 через find_package
    find_package(Catch2 REQUIRED)
    target_link_libraries(game_server_tests PRIVATE Catch2::Catch2 Threads::Threads)
endif()

# Включаем тесты
enable_testing()
add_test(NAME game_server_tests COMMAND game_server_tests)