FROM ubuntu:22.04

# Устанавливаем необходимые пакеты
RUN apt-get update && apt-get install -y \
    cmake \
    g++ \
    make \
    libboost-all-dev \
    python3-pip \
    git \
    && rm -rf /var/lib/apt/lists/*

# Устанавливаем conan
RUN pip3 install conan

# Устанавливаем рабочую директорию
WORKDIR /app

# Копируем содержимое папки precode (ваш проект)
COPY precode/ .

# Устанавливаем зависимости через conan
RUN mkdir build && cd build && \
    conan install .. --build=missing && \
    cmake .. && \
    make

# Запуск тестов
CMD ["./build/game_server_tests"]