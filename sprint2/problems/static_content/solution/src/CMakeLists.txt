cmake_minimum_required(VERSION 3.14)
project(game_server CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Опционально: подключаем conan paths если файл существует
if(EXISTS ${CMAKE_BINARY_DIR}/conan_paths.cmake)
    include(${CMAKE_BINARY_DIR}/conan_paths.cmake)
endif()

# Добавляем исполняемый файл
add_executable(game_server 
    src/main.cpp
)

# Устанавливаем выходную директорию
set_target_properties(game_server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Для работы с JSON
target_compile_definitions(game_server PRIVATE 
    USE_JSON=0
)