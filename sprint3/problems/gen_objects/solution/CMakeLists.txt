cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Находим зависимости
find_package(Catch2 REQUIRED)
find_package(Threads REQUIRED)
find_package(Boost REQUIRED COMPONENTS json) # Ищем Boost и нужный компонент

# Основное приложение
add_executable(game_server
    src/main.cpp
    src/model.cpp
    src/loot_generator.cpp
    src/extra_data.cpp
)

# Явно линкуем Boost и потоки к основному приложению
target_link_libraries(game_server PRIVATE Boost::json Threads::Threads)

# Тестовое приложение
add_executable(game_server_tests
    tests/catch_test_main.cpp
    tests/model-tests.cpp
    tests/loot_generator_tests.cpp
    src/model.cpp
    src/loot_generator.cpp
)

target_include_directories(game_server_tests PRIVATE src)

# Линкуем Catch2, потоки и Boost (если в тестах используются модели с JSON)
target_link_libraries(game_server_tests PRIVATE Catch2::Catch2 Threads::Threads Boost::json)

# Включаем тесты
enable_testing()
add_test(NAME game_server_tests COMMAND game_server_tests)