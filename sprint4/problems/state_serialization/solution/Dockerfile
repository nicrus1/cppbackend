FROM ubuntu:22.04

# Установка зависимостей
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    libboost-all-dev \
    python3-pip \
    git \
    wget \
    && rm -rf /var/lib/apt/lists/*

# Установка conan
RUN pip3 install conan

# Создаем рабочую директорию
WORKDIR /app

# Копируем исходники
COPY . .

# Сборка проекта
RUN mkdir -p build && cd build && \
    conan install .. --build missing && \
    cmake .. && \
    make -j$(nproc)

# Точка входа
ENTRYPOINT ["/app/build/game_server"]