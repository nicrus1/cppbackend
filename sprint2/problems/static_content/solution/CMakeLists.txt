cmake_minimum_required(VERSION 3.9)
project(HelloLog CXX)
set(CMAKE_CXX_STANDARD 20)

# Обратите внимание на аргумент TARGETS у команды conan_basic_setup
include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)

# Добавляем find_package для Boost
find_package(Boost REQUIRED COMPONENTS system)

add_executable(game_server main.cpp)

# Используем "импортированную" цель CONAN_PKG::boost
target_include_directories(game_server PRIVATE CONAN_PKG::boost)
target_link_libraries(game_server CONAN_PKG::boost ${Boost_LIBRARIES})

# Для линковки с pthread на Linux
if(UNIX)
    target_link_libraries(game_server pthread)
endif()