cmake_minimum_required(VERSION 3.10)
project(GameServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Исходники
set(SOURCES
    main.cpp
    api_handler.cpp
    game_session.cpp
    player_tokens.cpp
    player.cpp
)

# Заголовки
set(HEADERS
    api_handler.h
    game_session.h
    player_tokens.h
    player.h
)

# Создаём исполняемый файл
add_executable(game_server ${SOURCES} ${HEADERS})

# Добавляем pthread
find_package(Threads REQUIRED)
target_link_libraries(game_server Threads::Threads)

# Для Linux слинкуем дополнительные библиотеки
if(UNIX AND NOT APPLE)
    target_link_libraries(game_server pthread)
endif()

# Копируем статику и конфиги в build директорию (только если они существуют)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/static)
    add_custom_command(TARGET game_server POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/static
            ${CMAKE_CURRENT_BINARY_DIR}/static
    )
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/data)
    add_custom_command(TARGET game_server POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/data
            ${CMAKE_CURRENT_BINARY_DIR}/data
    )
endif()

# Выводим информацию
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")