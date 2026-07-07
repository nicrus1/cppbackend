cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Threads first
find_package(Threads REQUIRED)

# Включаем директорию с исходниками
set(SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Если в вашем conanfile.txt указан генератор "cmake", а не "cmake_multi", 
# замените эту строку на: include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
include(${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
conan_basic_setup(TARGETS)

# Основной исполняемый файл сервера
add_executable(game_server
    ${SOURCE_DIR}/main.cpp
    ${SOURCE_DIR}/model.cpp
    ${SOURCE_DIR}/json_loader.cpp
    ${SOURCE_DIR}/request_handler.cpp
    ${SOURCE_DIR}/game_state.cpp
    ${SOURCE_DIR}/loot_manager.cpp
    ${SOURCE_DIR}/loot_generator.cpp
    ${SOURCE_DIR}/http_server.cpp
    ${SOURCE_DIR}/logger.cpp
    ${SOURCE_DIR}/htmldecode.cpp
    ${SOURCE_DIR}/boost_json.cpp
)

target_include_directories(game_server PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Правильные имена целей от Conan
target_link_libraries(game_server PRIVATE 
    ${CONAN_TARGETS}
    Threads::Threads
)

# Тесты
add_executable(game_server_tests
    ${SOURCE_DIR}/model.cpp
    ${SOURCE_DIR}/game_state.cpp
    ${SOURCE_DIR}/loot_manager.cpp
    ${SOURCE_DIR}/loot_generator.cpp
    ${SOURCE_DIR}/model_serialization.h
    ${SOURCE_DIR}/player.h
    ${SOURCE_DIR}/players.h
    ${SOURCE_DIR}/player_tokens.h
    ${SOURCE_DIR}/tagged.h
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/state-serialization-tests.cpp
)

target_include_directories(game_server_tests PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
)

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