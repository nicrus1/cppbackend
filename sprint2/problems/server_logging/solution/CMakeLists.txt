cmake_minimum_required(VERSION 3.9)
project(LoggingServer CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost REQUIRED COMPONENTS log log_setup json system thread)

add_executable(logging_server 
    server.cpp
)

target_include_directories(logging_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(logging_server 
    Boost::log 
    Boost::log_setup 
    Boost::json 
    Boost::system 
    Boost::thread
)