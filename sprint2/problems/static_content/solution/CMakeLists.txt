cmake_minimum_required(VERSION 3.10)

# Указываем компилятор
if(NOT CMAKE_CXX_COMPILER)
    set(CMAKE_CXX_COMPILER g++)
endif()
set(CMAKE_C_COMPILER_WORKS 1)

# Проект только на C++
project(game_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Добавляем исполняемый файл
add_executable(game_server src/main.cpp)

# Устанавливаем выходную директорию
set_target_properties(game_server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Отключаем предупреждения о C компиляторе
set(CMAKE_C_COMPILER_WORKS 1)