FROM ubuntu:22.04 AS builder

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    g++ \
    make \
    wget \
    && rm -rf /var/lib/apt/lists/*

RUN wget https://github.com/nlohmann/json/releases/download/v3.11.2/json.hpp \
    -O /usr/local/include/json.hpp

WORKDIR /app

COPY CMakeLists.txt .
COPY *.cpp *.h ./

RUN mkdir -p data static

COPY data/ ./data/
COPY static/ ./static/

# Создаём пустой player.cpp если его нет (опционально)
RUN if [ ! -f player.cpp ]; then touch player.cpp; fi

RUN mkdir build && cd build && \
    cmake .. && \
    make && \
    cp game_server /app/game_server

FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

RUN useradd -m -s /bin/bash gameuser

COPY --from=builder /app/game_server /app/game_server
COPY --from=builder /app/data /app/data
COPY --from=builder /app/static /app/static

RUN chown -R gameuser:gameuser /app

USER gameuser
WORKDIR /app

EXPOSE 8080

CMD ["./game_server"]