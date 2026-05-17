FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    g++-11 \
    cmake \
    python3-pip \
    && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-11 100 \
    && pip3 install conan \
    && rm -rf /var/lib/apt/lists/*

# Создаём профиль Conan
RUN conan profile detect --force

WORKDIR /app

COPY . .

# Сборка
RUN mkdir -p build && cd build && \
    conan install .. --build=missing && \
    cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=20 && \
    cmake --build .

EXPOSE 8080

CMD ["./build/server"]