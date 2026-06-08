import argparse
import subprocess
import time
import random
import shlex
import os
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

PERF_DATA_FILE = 'perf.data'
FLAMEGRAPH_SVG = 'graph.svg'
FLAMEGRAPH_DIR = './FlameGraph'


def start_server():
    parser = argparse.ArgumentParser()
    parser.add_argument('server', type=str)
    return parser.parse_args().server


def run(command, output=None, input_data=None):
    """Запускает процесс с командой command"""
    if isinstance(command, str):
        command = shlex.split(command)
    process = subprocess.Popen(command, stdout=output, stderr=subprocess.DEVNULL, stdin=input_data)
    return process


def stop(process, wait=False):
    """Останавливает процесс"""
    if process.poll() is None:
        if wait:
            process.wait()
        else:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()


def shoot(ammo):
    """Выполняет один запрос к серверу"""
    hit = run('curl -s ' + ammo, output=subprocess.DEVNULL)
    time.sleep(COOLDOWN)
    stop(hit, wait=True)


def make_shots():
    """Выполняет серию запросов к серверу"""
    for _ in range(SHOOT_COUNT):
        ammo_number = random.randrange(RANDOM_LIMIT) % len(AMMUNITION)
        shoot(AMMUNITION[ammo_number])
    print('Shooting complete')


def get_server_pid(server_process):
    """Получает PID процесса сервера"""
    # Для процессов, запущенных через shell, может потребоваться дополнительная логика
    # Но для простоты считаем, что server_process.pid - это PID сервера
    return server_process.pid


def build_flamegraph():
    """Строит флеймграф из данных perf.data"""
    try:
        # Проверяем, существует ли директория с FlameGraph скриптами
        stackcollapse_path = os.path.join(FLAMEGRAPH_DIR, 'stackcollapse-perf.pl')
        flamegraph_path = os.path.join(FLAMEGRAPH_DIR, 'flamegraph.pl')
        
        if not os.path.exists(stackcollapse_path) or not os.path.exists(flamegraph_path):
            print(f"FlameGraph scripts not found in {FLAMEGRAPH_DIR}")
            return False
        
        # Запускаем perf script и передаем через пайп в stackcollapse-perf.pl, затем в flamegraph.pl
        # Используем subprocess.PIPE для создания пайпа между процессами
        perf_script = subprocess.Popen(
            ['perf', 'script', '-i', PERF_DATA_FILE],
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL
        )
        
        stackcollapse = subprocess.Popen(
            [stackcollapse_path],
            stdin=perf_script.stdout,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL
        )
        # Закрываем pipe в parent после fork
        perf_script.stdout.close()
        
        flamegraph = subprocess.Popen(
            [flamegraph_path],
            stdin=stackcollapse.stdout,
            stdout=open(FLAMEGRAPH_SVG, 'w'),
            stderr=subprocess.DEVNULL
        )
        stackcollapse.stdout.close()
        
        # Ждем завершения всех процессов
        flamegraph.communicate()
        
        print(f'Flamegraph saved to {FLAMEGRAPH_SVG}')
        return True
        
    except Exception as e:
        print(f'Error building flamegraph: {e}')
        return False


def main():
    # Удаляем старые файлы, если они существуют
    if os.path.exists(PERF_DATA_FILE):
        os.remove(PERF_DATA_FILE)
    if os.path.exists(FLAMEGRAPH_SVG):
        os.remove(FLAMEGRAPH_SVG)
    
    # Запускаем сервер
    server_command = start_server()
    print(f'Starting server: {server_command}')
    server = run(server_command)
    
    # Даем серверу время на запуск
    time.sleep(2)
    
    # Запускаем perf record для отслеживания процесса сервера
    # Используем -p для указания PID процесса
    # -g для записи информации о стеке вызовов
    # -F 99 для частоты семплирования 99 Гц
    # -o для указания выходного файла
    # -- sleep 30 для автоматической остановки через 30 секунд
    
    # Получаем PID сервера
    server_pid = server.pid
    print(f'Server PID: {server_pid}')
    
    # Запускаем perf record в фоновом режиме
    # Увеличиваем время записи, чтобы покрыть все выстрелы
    # (SHOOT_COUNT * COOLDOWN + 5 секунд запас)
    record_duration = SHOOT_COUNT * COOLDOWN + 10
    
    perf_cmd = [
        'perf', 'record',
        '-o', PERF_DATA_FILE,
        '-g',  # Записываем информацию о стеке вызовов
        '-F', '99',  # Частота семплирования 99 Гц
        '-p', str(server_pid),
        '--', 'sleep', str(record_duration)
    ]
    
    print(f'Starting perf record with command: {" ".join(perf_cmd)}')
    perf_process = run(perf_cmd)
    
    # Даем perf record время на инициализацию
    time.sleep(1)
    
    # Выполняем обстрел сервера запросами
    make_shots()
    
    # Ждем завершения perf record (он завершится через record_duration секунд)
    print('Waiting for perf record to finish...')
    stop(perf_process, wait=True)
    
    # Останавливаем сервер
    print('Stopping server...')
    stop(server)
    
    # Проверяем, что perf.data создан и не пуст
    if os.path.exists(PERF_DATA_FILE) and os.path.getsize(PERF_DATA_FILE) > 0:
        print(f'Perf data collected successfully ({os.path.getsize(PERF_DATA_FILE)} bytes)')
        
        # Строим флеймграф
        print('Building flamegraph...')
        if build_flamegraph():
            # Проверяем, содержит ли флеймграф вызовы RequestHandler
            if os.path.exists(FLAMEGRAPH_SVG):
                with open(FLAMEGRAPH_SVG, 'r') as f:
                    content = f.read()
                    if 'RequestHandler' in content:
                        print('Flamegraph contains RequestHandler methods')
                    else:
                        print('Warning: Flamegraph may not contain RequestHandler methods')
    else:
        print('Error: perf.data not created or empty')
    
    time.sleep(1)
    print('Job done')


if __name__ == '__main__':
    main()
