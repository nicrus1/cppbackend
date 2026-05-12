cmake_minimum_required(VERSION 3.14)

# Указываем что используем только C++ (не требуем C компилятор)
project(game_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Conan paths (если есть)
if(EXISTS ${CMAKE_BINARY_DIR}/conan_paths.cmake)
    include(${CMAKE_BINARY_DIR}/conan_paths.cmake)
endif()

# Пытаемся найти nlohmann_json, но не падаем если нет
find_package(nlohmann_json QUIET)

# Добавляем исполняемый файл
add_executable(game_server src/main.cpp)

# Если нашли nlohmann_json - линкуем
if(TARGET nlohmann_json::nlohmann_json)
    target_link_libraries(game_server nlohmann_json::nlohmann_json)
else()
    # Если нет библиотеки, используем встроенный JSON парсер
    target_compile_definitions(game_server PRIVATE USE_SIMPLE_JSON_PARSER)
endif()

set_target_properties(game_server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)