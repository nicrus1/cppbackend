# CMakeLists.txt (обновлённый для включения Boost.Asio)
cmake_minimum_required(VERSION 3.11)

project(Radio CXX)
set(CMAKE_CXX_STANDARD 20)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

find_package(Boost 1.78.0 REQUIRED COMPONENTS system)
if(Boost_FOUND)
  include_directories(${Boost_INCLUDE_DIRS})
  link_directories(${Boost_LIBRARY_DIRS})
endif()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

if(WIN32)
  add_definitions(-DNOMINMAX)
endif()

add_executable(radio src/main.cpp src/audio.h)
target_link_libraries(radio PRIVATE Threads::Threads ${Boost_LIBRARIES})