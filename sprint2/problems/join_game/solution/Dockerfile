# Этап 1: Сборка приложения
FROM ubuntu:22.04 AS builder

# Устанавливаем зависимости для сборки
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    g++ \
    make \
    wget \
    && rm -rf /var/lib/apt/lists/*

# Устанавливаем nlohmann/json (заголовочная библиотека)
RUN wget https://github.com/nlohmann/json/releases/download/v3.11.2/json.hpp \
    -O /usr/local/include/json.hpp

# Создаём рабочую директорию
WORKDIR /app

# Копируем исходники
COPY CMakeLists.txt .
COPY *.cpp *.h ./

# Создаём папки для данных
RUN mkdir -p data static

# Копируем конфиг и статические файлы
COPY data/ ./data/
COPY static/ ./static/

# Собираем проект
RUN mkdir build && cd build && \
    cmake .. && \
    make && \
    cp game_server /app/game_server

# Этап 2: Финальный образ (минимальный)
FROM ubuntu:22.04

# Устанавливаем runtime зависимости
RUN apt-get update && apt-get install -y \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Создаём пользователя для безопасности
RUN useradd -m -s /bin/bash gameuser

# Копируем собранный сервер и данные из builder
COPY --from=builder /app/game_server /app/game_server
COPY --from=builder /app/data /app/data
COPY --from=builder /app/static /app/static

# Устанавливаем владельца
RUN chown -R gameuser:gameuser /app

# Переключаемся на непривилегированного пользователя
USER gameuser

# Рабочая директория
WORKDIR /app

# Порт для HTTP сервера
EXPOSE 8080

# Запускаем сервер
CMD ["./game_server"]