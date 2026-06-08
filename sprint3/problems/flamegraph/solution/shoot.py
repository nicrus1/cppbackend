import argparse
import subprocess
import time
import random
import shlex
import signal

RANDOM_LIMIT = 1000
SEED = 123456789
random.seed(SEED)

AMMUNITION = [
    'localhost:8080/api/v1/maps/map1',
    'localhost:8080/api/v1/maps'
]

SHOOT_COUNT = 100
COOLDOWN = 0.1


def start_server():
    parser = argparse.ArgumentParser()
    parser.add_argument('server', type=str)
    return parser.parse_args().server


def run(command, output=None):
    return subprocess.Popen(
        shlex.split(command),
        stdout=output,
        stderr=subprocess.DEVNULL
    )


def stop(process, wait=False):
    if process.poll() is None:
        process.terminate()
        if wait:
            process.wait()


def shoot(ammo):
    hit = run("curl " + ammo, output=subprocess.DEVNULL)
    time.sleep(COOLDOWN)
    stop(hit, wait=True)


def make_shots():
    for _ in range(SHOOT_COUNT):
        ammo = random.randrange(RANDOM_LIMIT) % len(AMMUNITION)
        shoot(AMMUNITION[ammo])

    print("Shooting complete")


# Запуск сервера
server = run(start_server())

# Даем серверу стартовать
time.sleep(1)

# Запуск perf
perf = run(
    f"perf record -g -o perf.data -p {server.pid}"
)

# Выполняем запросы
make_shots()

# Корректно завершаем perf
if perf.poll() is None:
    perf.send_signal(signal.SIGINT)
    perf.wait()

# Останавливаем сервер
stop(server, wait=True)

# Строим flamegraph
script = subprocess.Popen(
    shlex.split("perf script -i perf.data"),
    stdout=subprocess.PIPE
)

collapse = subprocess.Popen(
    shlex.split("./FlameGraph/stackcollapse-perf.pl"),
    stdin=script.stdout,
    stdout=subprocess.PIPE
)

with open("graph.svg", "w") as graph:
    flame = subprocess.Popen(
        shlex.split("./FlameGraph/flamegraph.pl"),
        stdin=collapse.stdout,
        stdout=graph
    )
    flame.wait()

script.wait()
collapse.wait()

print("Job done")