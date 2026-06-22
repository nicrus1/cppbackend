
cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)

find_package(Boost REQUIRED)
find_package(Catch2 REQUIRED)

add_library(collision_detection_lib STATIC
	src/collision_detector.h
	src/collision_detector.cpp
)

target_link_libraries(collision_detection_lib PUBLIC Boost::boost Threads::Threads)

add_executable(collision_detection_tests
	tests/collision-detector-tests.cpp
)

target_link_libraries(collision_detection_tests Catch2::Catch2 collision_detection_lib)
