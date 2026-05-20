cmake_minimum_required(VERSION 3.10)
project(GameServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Исходники
set(SOURCES
    main.cpp
    api_handler.cpp
    game_session.cpp
    player_tokens.cpp
)

add_executable(game_server ${SOURCES})

# Добавляем pthread для Linux
find_package(Threads REQUIRED)
target_link_libraries(game_server Threads::Threads)

# Копируем статику и конфиги в build директорию
add_custom_command(TARGET game_server POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/static
        ${CMAKE_CURRENT_BINARY_DIR}/static
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/data
        ${CMAKE_CURRENT_BINARY_DIR}/data
)