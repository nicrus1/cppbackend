cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

find_package(Boost 1.78.0 REQUIRED COMPONENTS program_options)
if(Boost_FOUND)
  include_directories(${Boost_INCLUDE_DIRS})
  link_directories(${Boost_LIBRARY_DIRS})
endif()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_executable(game_server
    src/main.cpp
    src/http_server.cpp
    src/model.cpp
    src/json_loader.cpp
    src/request_handler.cpp
    src/logger.cpp
    src/boost_json.cpp
    src/game_state.cpp
    src/http_server.h
    src/model.h
    src/tagged.h
    src/json_loader.h
    src/request_handler.h
    src/logger.h
    src/player.h
    src/players.h
    src/player_tokens.h
    src/game_state.h
    src/api_handler.h
    src/dog.h
    src/road_map.h
)

target_link_libraries(game_server PRIVATE 
    Threads::Threads 
    ${Boost_LIBRARIES}
    boost_program_options
)

# Добавляем путь к библиотекам Boost от Conan
target_link_directories(game_server PRIVATE ${Boost_LIBRARY_DIRS})

target_include_directories(game_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)