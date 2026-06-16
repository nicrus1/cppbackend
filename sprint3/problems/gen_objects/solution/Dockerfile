FROM ubuntu:22.04

# Установка системных зависимостей
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    python3 \
    python3-pip \
    && rm -rf /var/lib/apt/lists/*

# Установка Conan (установится актуальная версия 2.x)
RUN pip3 install conan

WORKDIR /app

# Копируем conanfile.txt первым для кэширования Docker
COPY conanfile.txt .

# Создаем профиль по умолчанию и устанавливаем зависимости
# Явно указываем стандарт C++20, чтобы избежать конфликтов ABI
RUN conan profile detect --force && \
    conan install . --build=missing -s build_type=Release -s compiler.cppstd=20

# Копируем остальной исходный код
COPY . .

# Создаем директорию сборки, передаем тулчейн от Conan и собираем проект
RUN mkdir -p build && cd build && \
    cmake .. -DCMAKE_TOOLCHAIN_FILE=../conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release && \
    make

# Запускаем тесты
RUN cd build && ctest --output-on-failure

# Точка входа
CMD ["./build/game_server", "config.json"]