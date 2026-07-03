FROM gcc:11.3 AS build

# Установка необходимых пакетов
RUN apt update && \
    apt install -y python3-pip cmake && \
    pip3 install conan==1.*

# Настройка профиля Conan
RUN conan profile new default --detect --force && \
    conan profile update settings.compiler.libcxx=libstdc++11 default

# Копирование файлов проекта
COPY conanfile.txt /app/
COPY CMakeLists.txt /app/
COPY src/ /app/src/
COPY tests/ /app/tests/

# Сборка проекта
RUN mkdir -p /app/build && \
    cd /app/build && \
    conan install .. --build=missing && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build . --target game_server

# Финальный образ
FROM ubuntu:22.04

# Установка runtime зависимостей
RUN apt update && \
    apt install -y libstdc++6 ca-certificates && \
    rm -rf /var/lib/apt/lists/*

# Создание пользователя для запуска
RUN groupadd -r www && useradd -r -g www www

# Копирование исполняемого файла
COPY --from=build /app/build/bin/game_server /app/game_server

# Копирование файлов конфигурации и статики
COPY data/ /app/data/
COPY static/ /app/static/

# Настройка рабочей директории
WORKDIR /app

# Права на выполнение
RUN chmod +x /app/game_server

# Запуск сервера с параметрами по умолчанию
ENTRYPOINT ["/app/game_server"]
CMD ["--config-file", "/app/data/config.json", "--www-root", "/app/static"]