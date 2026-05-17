cmake_minimum_required(VERSION 3.9)
project(server CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Подключаем Conan toolchain если есть
if(EXISTS ${CMAKE_BINARY_DIR}/conan_toolchain.cmake)
    include(${CMAKE_BINARY_DIR}/conan_toolchain.cmake)
endif()

find_package(Boost REQUIRED COMPONENTS log log_setup system thread json)

add_executable(server server.cpp)

target_link_libraries(server 
    Boost::log 
    Boost::log_setup 
    Boost::system 
    Boost::thread 
    Boost::json
)