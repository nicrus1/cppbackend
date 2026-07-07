cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(Threads REQUIRED)
find_package(Boost REQUIRED COMPONENTS system filesystem)

# Если conan не используется, используем системный boost
if(NOT EXISTS ${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
    message(STATUS "Conan not found, using system boost")
    find_package(Boost REQUIRED COMPONENTS system filesystem serialization)
endif()

# Подключаем conan если есть
if(EXISTS ${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
    include(${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
    conan_basic_setup(TARGETS)
endif()

set(SOURCE_FILES
    src/main.cpp
)

set(LIBRARY_FILES
    src/model.cpp
)

set(HEADER_FILES
    src/geom.h
    src/model_serialization.h
    src/model.h
    src/tagged.h
    src/app_listener.h
    src/serializing_listener.h
    src/game.h
)

add_library(game_model STATIC ${LIBRARY_FILES} ${HEADER_FILES})

target_link_libraries(game_model PUBLIC 
    Threads::Threads
    ${Boost_LIBRARIES}
)

if(EXISTS ${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
    target_link_libraries(game_model PUBLIC CONAN_PKG::boost)
endif()

add_executable(game_server ${SOURCE_FILES})

target_link_libraries(game_server game_model)

if(EXISTS ${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
    target_link_libraries(game_server CONAN_PKG::boost)
endif()

# Добавляем тесты
enable_testing()
add_executable(game_server_tests
    tests/state-serialization-tests.cpp
)

target_link_libraries(game_server_tests game_model)

if(EXISTS ${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
    target_link_libraries(game_server_tests CONAN_PKG::catch2)
else()
    find_package(Catch2 REQUIRED)
    target_link_libraries(game_server_tests Catch2::Catch2)
endif()