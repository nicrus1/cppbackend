FROM ubuntu:22.04

# Устанавливаем зависимости
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    libboost-all-dev \
    && rm -rf /var/lib/apt/lists/*

# Создаем рабочую директорию
WORKDIR /app

# Копируем исходники
COPY . /app

# Собираем проект
RUN mkdir build && cd build && \
    cmake .. && \
    make

# Копируем конфиг и статику
COPY config.json /app/data/config.json
COPY static /app/static/

# Открываем порт
EXPOSE 8080

# Запускаем сервер
CMD ["./build/game_server", "--config-file", "/app/data/config.json", "--www-root", "/app/static/"]