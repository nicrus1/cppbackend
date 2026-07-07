FROM ubuntu:22.04

# Установка зависимостей
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    libboost-all-dev \
    && rm -rf /var/lib/apt/lists/*

# Создаем рабочую директорию
WORKDIR /app

# Копируем исходники
COPY . .

# Сборка проекта
RUN mkdir build && cd build && \
    cmake .. && \
    make -j$(nproc)

# Точка входа
ENTRYPOINT ["/app/build/game_server"]