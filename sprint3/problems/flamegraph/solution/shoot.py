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


def stop(process, wait=False, kill=False):
    """Останавливает процесс"""
    if process.poll() is None:
        if kill:
            process.kill()
        elif wait:
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


def build_flamegraph():
    """Строит флеймграф из данных perf.data"""
    try:
        # Проверяем, существует ли директория с FlameGraph скриптами
        stackcollapse_path = os.path.join(FLAMEGRAPH_DIR, 'stackcollapse-perf.pl')
        flamegraph_path = os.path.join(FLAMEGRAPH_DIR, 'flamegraph.pl')
        
        if not os.path.exists(stackcollapse_path) or not os.path.exists(flamegraph_path):
            print(f"FlameGraph scripts not found in {FLAMEGRAPH_DIR}")
            return False
        
        # Запускаем perf script и передаем через пайп
        # Используем -f для принудительного вывода даже если данные неполные
        perf_script = subprocess.Popen(
            ['perf', 'script', '-i', PERF_DATA_FILE, '-f'],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )
        
        stackcollapse = subprocess.Popen(
            [stackcollapse_path],
            stdin=perf_script.stdout,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )
        perf_script.stdout.close()
        
        with open(FLAMEGRAPH_SVG, 'w') as output_file:
            flamegraph = subprocess.Popen(
                [flamegraph_path],
                stdin=stackcollapse.stdout,
                stdout=output_file,
                stderr=subprocess.PIPE,
                text=True
            )
            stackcollapse.stdout.close()
            
            # Ждем завершения
            flamegraph.communicate()
            
            # Проверяем, не было ли ошибок
            if flamegraph.returncode != 0:
                print(f'flamegraph.pl failed with return code {flamegraph.returncode}')
                return False
        
        print(f'Flamegraph saved to {FLAMEGRAPH_SVG}')
        return True
        
    except Exception as e:
        print(f'Error building flamegraph: {e}')
        return False


def main():
    # Удаляем старые файлы, если они существуют
    for f in [PERF_DATA_FILE, FLAMEGRAPH_SVG]:
        if os.path.exists(f):
            os.remove(f)
    
    # Запускаем сервер
    server_command = start_server()
    print(f'Starting server: {server_command}')
    server = run(server_command)
    
    # Даем серверу время на запуск
    time.sleep(2)
    
    # Получаем PID сервера
    server_pid = server.pid
    print(f'Server PID: {server_pid}')
    
    # Запускаем perf record для сбора данных в течение 5 секунд после завершения запросов
    # и отправляем его в фон
    perf_cmd = [
        'perf', 'record',
        '-o', PERF_DATA_FILE,
        '-g',  # Записываем информацию о стеке вызовов
        '-F', '99',  # Частота семплирования 99 Гц
        '-p', str(server_pid),
        '-e', 'cpu-clock',  # Явно указываем событие
        '--', 'sleep', '30'  # Записываем 30 секунд
    ]
    
    print(f'Starting perf record with command: {" ".join(perf_cmd)}')
    perf_process = run(perf_cmd)
    
    # Даем perf record время на инициализацию
    time.sleep(2)
    
    # Выполняем обстрел сервера запросами
    make_shots()
    
    # Даем perf record еще немного времени после завершения запросов
    print('Waiting for perf record to collect samples...')
    time.sleep(3)
    
    # Останавливаем perf record
    print('Stopping perf record...')
    stop(perf_process)
    
    # Ждем завершения perf record и сохранения данных
    time.sleep(2)
    
    # Останавливаем сервер
    print('Stopping server...')
    stop(server)
    
    time.sleep(1)
    
    # Проверяем, что perf.data создан и не пуст
    if os.path.exists(PERF_DATA_FILE) and os.path.getsize(PERF_DATA_FILE) > 0:
        print(f'Perf data collected successfully ({os.path.getsize(PERF_DATA_FILE)} bytes)')
        
        # Проверяем, есть ли данные через perf report
        check_result = subprocess.run(
            ['perf', 'report', '-i', PERF_DATA_FILE, '--stdio', '--sort=comm', '-n'],
            capture_output=True,
            text=True
        )
        print(f'Perf report sample: {check_result.stdout[:500]}')
        
        # Строим флеймграф
        print('Building flamegraph...')
        if build_flamegraph():
            # Проверяем результат
            if os.path.exists(FLAMEGRAPH_SVG) and os.path.getsize(FLAMEGRAPH_SVG) > 0:
                with open(FLAMEGRAPH_SVG, 'r') as f:
                    content = f.read()
                    if 'ERROR: No valid input' in content:
                        print('Error: flamegraph.pl did not receive valid input')
                    elif 'RequestHandler' in content or 'http_handler::RequestHandler' in content:
                        print('Flamegraph contains RequestHandler methods')
                    else:
                        print('Warning: Flamegraph may not contain RequestHandler methods')
            else:
                print('Flamegraph file not created or empty')
    else:
        print(f'Error: perf.data not created or empty. Size: {os.path.getsize(PERF_DATA_FILE) if os.path.exists(PERF_DATA_FILE) else 0}')
    
    print('Job done')


if __name__ == '__main__':
    main()