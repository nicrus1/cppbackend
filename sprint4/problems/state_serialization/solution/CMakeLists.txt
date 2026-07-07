cmake_minimum_required(VERSION 3.15)
project(game_server CXX)

# Используем современный стандарт C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Включаем строгие предупреждения компилятора для чистоты кода
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -pedantic)
endif()

# 1. Поиск системных потоков (pthread)
find_package(Threads REQUIRED)

# 2. Поиск необходимых компонент Boost
# Обратите внимание: для работы Boost.Log также нужны компоненты log_setup и thread
find_package(Boost REQUIRED COMPONENTS
    system
    program_options
    serialization
    log
    log_setup
    thread
)

# 3. Список исходных файлов (добавьте сюда все ваши .cpp файлы)
set(SOURCES
    src/main.cpp
    src/model.cpp
    # src/http_server.cpp
    # src/json_loader.cpp
)

# 4. Создаем исполняемый файл
add_executable(game_server ${SOURCES})

# Подключаем директорию с заголовочными файлами (.h файлы лежат в src/)
target_include_directories(game_server PRIVATE src)

# 5. Линкуем библиотеки к нашему серверу
target_link_libraries(game_server PRIVATE
    Boost::system
    Boost::program_options
    Boost::serialization
    Boost::log
    Boost::log_setup
    Boost::thread
    Threads::Threads
)

# Важно: для корректной работы Boost.Log при динамической линковке на Linux
target_compile_definitions(game_server PRIVATE BOOST_LOG_DYN_LINK)