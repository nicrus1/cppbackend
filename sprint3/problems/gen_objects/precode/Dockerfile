FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    g++ \
    cmake \
    make \
    libboost-all-dev \
    python3-pip \
    git \
    && rm -rf /var/lib/apt/lists/*

RUN pip3 install conan

WORKDIR /app
COPY . .

RUN mkdir -p build && cd build && \
    conan install .. --build=missing && \
    cmake .. && \
    make

CMD ["./build/game_server_tests"]