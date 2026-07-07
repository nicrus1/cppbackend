cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Находим Boost
find_package(Boost REQUIRED COMPONENTS system filesystem program_options)
find_package(Threads REQUIRED)

# Все исходные файлы проекта
set(GAME_SOURCES
    src/model.cpp
    src/game_state.cpp
    src/loot_manager.cpp
    src/loot_generator.cpp
    src/http_server.cpp
    src/logger.cpp
    src/json_loader.cpp
    src/htmldecode.cpp
    src/request_handler.cpp
    src/extra_data.cpp
    src/boost_json.cpp
)

# Основная библиотека
add_library(game_model STATIC ${GAME_SOURCES})

target_include_directories(game_model PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${Boost_INCLUDE_DIRS}
)

target_link_libraries(game_model PUBLIC 
    Boost::system 
    Boost::filesystem 
    Boost::program_options
    Threads::Threads
)

# Исполняемый файл
add_executable(game_server
    src/main.cpp
)

target_include_directories(game_server PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${Boost_INCLUDE_DIRS}
)

target_link_libraries(game_server 
    game_model
    Boost::system 
    Boost::filesystem 
    Boost::program_options
    Threads::Threads
)