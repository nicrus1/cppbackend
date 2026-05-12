cmake_minimum_required(VERSION 3.14)
project(game_server)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Conan
include(${CMAKE_BINARY_DIR}/conan_paths.cmake)

# Находим nlohmann_json
find_package(nlohmann_json REQUIRED)

add_executable(game_server src/main.cpp)

# Линкуем с nlohmann_json
target_link_libraries(game_server nlohmann_json::nlohmann_json)

set_target_properties(game_server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)