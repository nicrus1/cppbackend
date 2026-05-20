# Не просто создаём образ, но даём ему имя build
FROM gcc:11.3 as build

RUN apt update && \
    apt install -y \
      python3-pip \
      cmake \
    && \
    pip3 install conan==1.*

# Запуск conan как раньше
COPY conanfile.txt /app/
RUN mkdir /app/build && cd /app/build && \
    conan install .. --build=missing

# Копируем исходники (все .cpp и .h файлы)
COPY *.cpp /app/src/
COPY *.h /app/src/
COPY CMakeLists.txt /app/
COPY conanfile.txt /app/

# Также копируем статические файлы, если они есть
COPY *.html /app/static/
COPY *.svg /app/static/images/ 2>/dev/null || true

# Создаём директорию для сборки и компилируем
RUN cd /app/build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build . --target game_server

# Второй контейнер в том же докерфайле
FROM ubuntu:22.04 as run

# Устанавливаем необходимые runtime библиотеки
RUN apt update && \
    apt install -y \
      libstdc++6 \
      ca-certificates \
    && \
    rm -rf /var/lib/apt/lists/*

# Создадим пользователя www
RUN groupadd -r www && useradd -r -g www www
USER www

# Скопируем приложение со сборочного контейнера
COPY --from=build /app/build/bin/game_server /app/game_server

# Копируем конфигурационные файлы и статику
COPY --chown=www:www config.json /app/data/config.json
COPY --chown=www:www *.html /app/static/
COPY --chown=www:www *.svg /app/static/images/ 2>/dev/null || true

# Создаём рабочую директорию
WORKDIR /app

# Запускаем игровой сервер
ENTRYPOINT ["/app/game_server", "/app/data/config.json"]