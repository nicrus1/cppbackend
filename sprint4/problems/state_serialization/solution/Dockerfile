FROM ubuntu:22.04 AS build

# Установка всех зависимостей для сборки
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        g++ \
        cmake \
        make \
        pkg-config \
        python3-pip \
        libboost-all-dev \
        libpqxx-dev \
        libpq-dev \
        && \
    pip3 install conan==1.* --no-cache-dir && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

# Настройка Conan (фиксируем профиль)
RUN conan profile new default --detect --force && \
    conan profile update settings.compiler.libcxx=libstdc++11 default

# Копирование файлов конфигурации и исходников
COPY conanfile.txt /app/
COPY CMakeLists.txt /app/
COPY ./src /app/src
COPY ./tests/ /app/tests
COPY ./data/ /app/data
COPY ./static/ /app/static

# Сборка проекта с отладкой Conan (установкой прекомпиляции для отсутствующих пакетов)
RUN mkdir -p /app/build && \
    cd /app/build && \
    conan install .. --build=missing && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build . --target game_server

# Финальный образ
FROM ubuntu:22.04

# Установка runtime зависимостей
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        libstdc++6 \
        ca-certificates \
        libpq5 \
        && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

# Создание пользователя для запуска
RUN groupadd -r www && useradd -r -g www www

# Копирование исполняемого файла и ресурсов
COPY --from=build /app/build/bin/game_server /app/game_server
COPY --from=build /app/data /app/data
COPY --from=build /app/static /app/static

# Создаем директорию для состояния
RUN mkdir -p /tmp/volume && \
    chown -R www:www /app /tmp/volume

WORKDIR /app
RUN chmod +x /app/game_server

ENTRYPOINT ["/app/game_server"]
CMD ["--config-file", "/app/data/config.json", "--www-root", "/app/static", "--state-file", "/tmp/volume/state.dat", "--save-state-period", "5000"]