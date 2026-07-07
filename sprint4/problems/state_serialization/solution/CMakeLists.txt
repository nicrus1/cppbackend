cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(Threads REQUIRED)
find_package(Boost REQUIRED COMPONENTS system filesystem serialization)

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

# Устанавливаем пути к исходникам
set(SOURCES_DIR ${CMAKE_SOURCE_DIR}/src)
set(TESTS_DIR ${CMAKE_SOURCE_DIR}/tests)

# Исходные файлы
set(SOURCE_FILES
    ${SOURCES_DIR}/main.cpp
)

set(LIBRARY_FILES
    ${SOURCES_DIR}/model.cpp
)

set(HEADER_FILES
    ${SOURCES_DIR}/geom.h
    ${SOURCES_DIR}/model_serialization.h
    ${SOURCES_DIR}/model.h
    ${SOURCES_DIR}/tagged.h
    ${SOURCES_DIR}/app_listener.h
    ${SOURCES_DIR}/serializing_listener.h
    ${SOURCES_DIR}/game.h
)

# Создаем библиотеку с моделью игры
add_library(game_model STATIC ${LIBRARY_FILES} ${HEADER_FILES})

# Добавляем include директории
target_include_directories(game_model PUBLIC ${SOURCES_DIR})

# Линкуем библиотеки
target_link_libraries(game_model PUBLIC 
    Threads::Threads
    ${Boost_LIBRARIES}
)

if(EXISTS ${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
    target_link_libraries(game_model PUBLIC CONAN_PKG::boost)
endif()

# Создаем исполняемый файл сервера
add_executable(game_server ${SOURCE_FILES})

# Линкуем библиотеки к серверу
target_link_libraries(game_server 
    game_model
    Threads::Threads
    ${Boost_LIBRARIES}
)

# Добавляем include директории для сервера
target_include_directories(game_server PRIVATE ${SOURCES_DIR})

if(EXISTS ${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
    target_link_libraries(game_server CONAN_PKG::boost)
endif()

# Добавляем тесты
enable_testing()

# Собираем тесты, если они есть
file(GLOB TEST_SOURCES ${TESTS_DIR}/*.cpp)

if(TEST_SOURCES)
    add_executable(game_server_tests ${TEST_SOURCES})
    
    target_link_libraries(game_server_tests 
        game_model
        Threads::Threads
        ${Boost_LIBRARIES}
    )
    
    target_include_directories(game_server_tests PRIVATE ${SOURCES_DIR})
    
    if(EXISTS ${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
        target_link_libraries(game_server_tests CONAN_PKG::catch2)
        target_link_libraries(game_server_tests CONAN_PKG::boost)
    else()
        # Если Catch2 не найден через conan, пытаемся найти системный
        find_package(Catch2 QUIET)
        if(Catch2_FOUND)
            target_link_libraries(game_server_tests Catch2::Catch2)
        else()
            message(WARNING "Catch2 not found, tests will not be linked")
        endif()
    endif()
    
    # Добавляем тесты в CTest
    add_test(NAME state_serialization_test COMMAND game_server_tests)
endif()

# Устанавливаем флаги компиляции для отладки
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(game_server PRIVATE -g -O0 -Wall -Wextra)
    target_compile_options(game_model PRIVATE -g -O0 -Wall -Wextra)
endif()

# Устанавливаем флаги компиляции для релиза
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(game_server PRIVATE -O3 -DNDEBUG)
    target_compile_options(game_model PRIVATE -O3 -DNDEBUG)
endif()

# Копируем конфигурационные файлы в бинарную директорию (опционально)
if(EXISTS ${CMAKE_SOURCE_DIR}/config.json)
    configure_file(${CMAKE_SOURCE_DIR}/config.json ${CMAKE_BINARY_DIR}/config.json COPYONLY)
endif()

# Выводим информацию о сборке
message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Source directory: ${SOURCES_DIR}")
message(STATUS "Tests directory: ${TESTS_DIR}")