FROM gcc:11.3 AS build

RUN apt update && \
    apt install -y python3-pip cmake && \
    pip3 install conan==1.*

RUN conan profile new default --detect --force && \
    conan profile update settings.compiler.libcxx=libstdc++11 default

COPY conanfile.txt /app/
COPY CMakeLists.txt /app/
COPY src/ /app/src/


RUN mkdir -p /app/build && \
    cd /app/build && \
    conan install .. --build=missing && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build . --target game_server

FROM ubuntu:22.04

RUN apt update && \
    apt install -y libstdc++6 ca-certificates && \
    rm -rf /var/lib/apt/lists/*

RUN groupadd -r www && useradd -r -g www www
USER www

COPY --from=build /app/build/bin/game_server /app/game_server

COPY data/ /app/data/
COPY static/ /app/static/

WORKDIR /app

ENTRYPOINT ["/app/game_server"]