cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Threads first
find_package(Threads REQUIRED)

# Если в вашем conanfile.txt указан генератор "cmake", а не "cmake_multi", 
# замените эту строку на: include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
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
    src/db/db_connection_pool.cpp
    src/db/record_manager.cpp
)

target_include_directories(game_server PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Правильные имена целей от Conan (изменено с CONAN_LIBS на CONAN_TARGETS)
target_link_libraries(game_server PRIVATE 
    ${CONAN_TARGETS}
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
)

target_include_directories(game_server_tests PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
)

# (изменено с CONAN_LIBS на CONAN_TARGETS)
target_link_libraries(game_server_tests PRIVATE 
    ${CONAN_TARGETS}
    Threads::Threads
)

# Настройка выходных директорий
set_target_properties(game_server game_server_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Включение тестирования
enable_testing()
add_test(NAME game_server_tests COMMAND game_server_tests)