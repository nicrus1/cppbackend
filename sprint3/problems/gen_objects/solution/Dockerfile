FROM ubuntu:22.04

# Установка системных зависимостей
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    python3 \
    python3-pip \
    libboost-all-dev \
    && rm -rf /var/lib/apt/lists/*

# Установка Conan
RUN pip3 install conan

WORKDIR /app

# Копируем conanfile.txt первым для кэширования Docker
COPY conanfile.txt .

# Запускаем Conan для генерации файлов сборки
RUN conan install . --build=missing

# Копируем остальной исходный код
COPY . .

# Создаем директорию сборки и собираем проект
RUN mkdir -p build && cd build && \
    cmake .. && \
    make

# Запускаем тесты
RUN cd build && ctest --output-on-failure

# Точка входа
CMD ["./build/game_server", "config.json"]