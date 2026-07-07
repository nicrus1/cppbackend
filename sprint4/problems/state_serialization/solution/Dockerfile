# ---------------------------------
# Этап 1: Сборка
# ---------------------------------
FROM gcc:11.3 AS build

RUN apt update && \
    apt install -y python3-pip cmake && \
    pip3 install conan==1.*

RUN conan profile new default --detect --force && \
    conan profile update settings.compiler.libcxx=libstdc++11 default

COPY conanfile.txt /app/
COPY CMakeLists.txt /app/
COPY src/ /app/src/
COPY tests/ /app/tests/
COPY data/ /app/data/
COPY static/ /app/static/

RUN mkdir -p /app/build && \
    cd /app/build && \
    conan install .. --build=missing && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build . --target game_server

# Добавьте проверку:
RUN ls -la /app/build/bin/ || ls -la /app/build/

# ---------------------------------
# Этап 2: Финальный образ
# ---------------------------------
FROM ubuntu:22.04

RUN apt update && \
    apt install -y \
        libstdc++6 \
        ca-certificates \
        && \
    rm -rf /var/lib/apt/lists/*

RUN groupadd -r www && useradd -r -g www www

# Попробуйте оба пути:
COPY --from=build /app/build/bin/game_server /app/game_server || \
     COPY --from=build /app/build/game_server /app/game_server

COPY data/ /app/data/
COPY static/ /app/static/

# Создаём и даём полные права:
RUN mkdir -p /tmp/volume && chmod 777 /tmp/volume

WORKDIR /app
RUN chmod +x /app/game_server

# Проверка перед запуском:
RUN ls -la /app/ && ls -la /app/data/

USER www

ENTRYPOINT ["/app/game_server"]
CMD ["--config-file", "/app/data/config.json", "--www-root", "/app/static", "--state-file", "/tmp/volume/state.dat", "--save-state-period", "5000"]