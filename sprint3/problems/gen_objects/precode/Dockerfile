FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    cmake \
    g++ \
    make \
    libboost-all-dev \
    python3-pip \
    git \
    && rm -rf /var/lib/apt/lists/*

RUN pip3 install conan

WORKDIR /app

# Копируем проект из папки precode (поднимаемся на 2 уровня вверх и заходим в precode)
COPY ../../precode/ .

# Показываем содержимое для отладки
RUN ls -la

RUN mkdir build && cd build && \
    conan install .. --build=missing && \
    cmake .. && \
    make

CMD ["./build/game_server_tests"]