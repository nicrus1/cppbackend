# 1. Этап сборки (Build)
FROM gcc:11.3 AS build

RUN apt update && \
    apt install -y python3-pip cmake && \
    pip3 install conan==1.*

# Настраиваем Conan (обязательно для GCC 11, чтобы избежать конфликтов ABI)
RUN conan profile detect --force && \
    conan profile update settings.compiler.libcxx=libstdc++11 default

# Копируем файлы конфигурации проекта
COPY conanfile.txt /app/
COPY CMakeLists.txt /app/
COPY src/ /app/src/

# ВАЖНО: Выполняем установку зависимостей и сборку в одном слое RUN, 
# чтобы сгенерированные Conan файлы были доступны для CMake
RUN mkdir /app/build && \
    cd /app/build && \
    conan install .. --build=missing && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build . --target game_server

# 2. Финальный этап (Run)
FROM ubuntu:22.04

# Устанавливаем runtime-зависимости
RUN apt update && \
    apt install -y libstdc++6 ca-certificates && \
    rm -rf /var/lib/apt/lists/*

# Создаем пользователя
RUN groupadd -r www && useradd -r -g www www
USER www

# Копируем бинарник из этапа сборки
COPY --from=build /app/build/bin/game_server /app/game_server

# Копируем ресурсы
COPY data/ /app/data/
COPY static/ /app/static/

WORKDIR /app

# ENTRYPOINT остается прежним, но помните, что дополнительные 
# аргументы (типа --tick-period) можно передавать при запуске docker run
ENTRYPOINT ["/app/game_server"]