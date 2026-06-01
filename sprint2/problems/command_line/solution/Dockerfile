# 1. Этап сборки (Build)
FROM gcc:11.3 AS build

RUN apt update && \
    apt install -y python3-pip cmake && \
    pip3 install conan==1.*

# Настройка профиля для Conan 1.x (создание дефолтного профиля и установка ABI)
RUN conan profile new default --detect --force && \
    conan profile update settings.compiler.libcxx=libstdc++11 default

# Копируем файлы, необходимые для Conan и сборки
COPY conanfile.txt /app/
COPY CMakeLists.txt /app/
COPY src/ /app/src/

# ВАЖНО: Выполняем установку зависимостей, генерацию файлов CMake и саму сборку в одном слое.
# Это гарантирует, что conanbuildinfo.cmake будет доступен для cmake.
RUN mkdir -p /app/build && \
    cd /app/build && \
    conan install .. --build=missing && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build . --target game_server

# 2. Финальный этап (Run)
FROM ubuntu:22.04

# Устанавливаем runtime-зависимости (libstdc++6 для работы бинарника)
RUN apt update && \
    apt install -y libstdc++6 ca-certificates && \
    rm -rf /var/lib/apt/lists/*

# Создаем пользователя www для безопасности
RUN groupadd -r www && useradd -r -g www www
USER www

# Копируем скомпилированный файл из контейнера build
# Убедитесь, что путь к бинарнику в вашем CMake соответствует /app/build/bin/game_server
COPY --from=build /app/build/bin/game_server /app/game_server

# Копируем конфигурацию и статические файлы
COPY data/ /app/data/
COPY static/ /app/static/

WORKDIR /app

# ENTRYPOINT без жестко заданного аргумента позволяет передавать параметры при docker run
# Пример: docker run game_server data/config.json -t 50
ENTRYPOINT ["/app/game_server"]