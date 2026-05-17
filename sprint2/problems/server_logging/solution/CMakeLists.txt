cmake_minimum_required(VERSION 3.15)
project(server CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Boost REQUIRED COMPONENTS
    system
    thread
    log
    log_setup
    json
)

add_executable(server
    server.cpp
)

target_include_directories(server PRIVATE ${Boost_INCLUDE_DIRS})

target_link_libraries(server PRIVATE
    Boost::system
    Boost::thread
    Boost::log
    Boost::log_setup
    Boost::json
    pthread
)