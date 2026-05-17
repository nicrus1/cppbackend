FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    g++ \
    cmake \
    python3-pip \
    && pip3 install conan \
    && rm -rf /var/lib/apt/lists/*

RUN conan profile detect --force

WORKDIR /app

COPY . .

RUN mkdir build && cd build && \
    conan install .. --build=missing -s build_type=Release && \
    cmake .. \
        -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake \
        -DCMAKE_BUILD_TYPE=Release && \
    cmake --build . --config Release

EXPOSE 8080

CMD ["./build/server"]