FROM ubuntu:22.04

# Устанавливаем необходимые пакеты
RUN apt-get update && apt-get install -y \
    cmake \
    g++ \
    make \
    libboost-all-dev \
    && rm -rf /var/lib/apt/lists/*

# Устанавливаем Catch2 из исходников
RUN git clone https://github.com/catchorg/Catch2.git && \
    cd Catch2 && \
    cmake -Bbuild -H. -DBUILD_TESTING=OFF && \
    cmake --build build/ --target install && \
    cd .. && \
    rm -rf Catch2

# Устанавливаем рабочую директорию
WORKDIR /app

# Копируем файлы проекта
COPY CMakeLists.txt .
COPY config.json .
COPY src/ src/
COPY tests/ tests/

# Собираем проект
RUN mkdir build && cd build && \
    cmake .. && \
    make

# Запуск тестов по умолчанию
CMD ["./build/game_server_tests"]