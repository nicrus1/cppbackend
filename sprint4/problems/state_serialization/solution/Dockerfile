# ==========================================
# Этап 1: Сборка (Builder)
# ==========================================
FROM ubuntu:22.04 AS builder

# Отключаем интерактивные запросы при установке пакетов через apt
ENV DEBIAN_FRONTEND=noninteractive

# Устанавливаем компилятор, CMake и полный набор библиотек Boost для сборки
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    libboost-all-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Копируем файлы сборки и исходный код
COPY CMakeLists.txt .
COPY src/ ./src/

# Если у вас есть папка с веб-ресурсами (статика) или конфигами, раскомментируйте:
# COPY www/ ./www/
# COPY data/ ./data/

# Собираем проект в режиме Release с максимальной оптимизацией
RUN mkdir build && cd build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build . --parallel $(nproc)

# ==========================================
# Этап 2: Запуск (Runtime)
# ==========================================
FROM ubuntu:22.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive

# Устанавливаем ТОЛЬКО runtime-библиотеки Boost (без компиляторов и .hpp файлов)
RUN apt-get update && apt-get install -y \
    libboost-system1.74.0 \
    libboost-program-options1.74.0 \
    libboost-serialization1.74.0 \
    libboost-log1.74.0 \
    libboost-thread1.74.0 \
    && rm -rf /var/lib/apt/lists/*

# Создаем непривилегированного пользователя для безопасности (best practice)
RUN groupadd -r gameserver && useradd -r -g gameserver gameserver

WORKDIR /app

# Копируем собранный исполняемый файл из первого этапа
COPY --from=builder /app/build/game_server /app/game_server

# Копируем ресурсы и конфигурации (если они нужны вашему серверу)
# COPY --from=builder /app/www /app/www
# COPY --from=builder /app/data /app/data

# Создаем папку для сохранения файла состояния игры (--state-file) и даем права
RUN mkdir -p /app/state && chown -R gameserver:gameserver /app

# Переключаемся на безопасного пользователя
USER gameserver

# Открываем порт, на котором слушает игровой сервер (например, 8080)
EXPOSE 8080

# Объявляем volume для папки с состоянием игры, чтобы данные не терялись при перезапуске
VOLUME ["/app/state"]

# Точка входа: запускаем сервер
ENTRYPOINT ["/app/game_server"]

# Аргументы по умолчанию (можно переопределить при вызове docker run)
CMD ["--config-file", "/app/data/config.json", \
     "--www-root", "/app/www", \
     "--state-file", "/app/state/gamestate.bin", \
     "--save-state-period", "5000"]