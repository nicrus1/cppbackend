FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    g++-11 \
    cmake \
    libboost-all-dev \
    && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-11 100 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY . .

RUN mkdir -p build && cd build && \
    cmake -DCMAKE_CXX_STANDARD=20 .. && \
    cmake --build .

CMD ["./build/server"]