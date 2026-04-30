cmake_minimum_required(VERSION 3.11)

# Проект называется Seabattle и написан на C++
project(Seabattle CXX)

# Исходный код будет компилироваться с поддержкой стандарта С++ 20
set(CMAKE_CXX_STANDARD 20)

# Подключаем сгенерированный скрипт conanbuildinfo.cmake, созданный Conan
include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
# Выполняем макрос из conanbuildinfo.cmake, который настроит CMake на работу с библиотеками, установленными Conan
conan_basic_setup()

# Ищем Boost версии 1.78
find_package(Boost 1.78.0 REQUIRED)
if(Boost_FOUND)
  # boost найден, добавляем к каталогам заголовочных файлов проекта путь к заголовочным файлам boost
  include_directories(${Boost_INCLUDE_DIRS})
endif()

# Платформы вроде linux требуют подключения библиотеки pthread для поддержки стандартных потоков.
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# Проект содержит исходный файл src/main.cpp
add_executable(seabattle src/main.cpp src/seabattle.h)

# Просим компоновщик подключить библиотеку для поддержки потоков и Boost::system
target_link_libraries(seabattle PRIVATE Threads::Threads ${Boost_LIBRARIES})