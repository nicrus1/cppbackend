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
PERF_SCRIPT_OUTPUT = 'perf.script'
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
    for i in range(SHOOT_COUNT):
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
        
        # Сначала запускаем perf script и сохраняем вывод во временный файл
        with open(PERF_SCRIPT_OUTPUT, 'w') as f:
            perf_script = subprocess.run(
                ['perf', 'script', '-i', PERF_DATA_FILE],
                stdout=f,
                stderr=subprocess.DEVNULL
            )
        
        if perf_script.returncode != 0:
            print('perf script failed')
            return False
        
        # Проверяем, что файл не пустой
        if os.path.getsize(PERF_SCRIPT_OUTPUT) == 0:
            print('perf script output is empty')
            return False
        
        # Теперь обрабатываем через stackcollapse и flamegraph
        with open(PERF_SCRIPT_OUTPUT, 'r') as input_file:
            stackcollapse = subprocess.Popen(
                [stackcollapse_path],
                stdin=input_file,
                stdout=subprocess.PIPE,
                stderr=subprocess.DEVNULL
            )
            
            with open(FLAMEGRAPH_SVG, 'w') as output_file:
                flamegraph = subprocess.Popen(
                    [flamegraph_path],
                    stdin=stackcollapse.stdout,
                    stdout=output_file,
                    stderr=subprocess.DEVNULL
                )
                
                # Закрываем pipe в родительском процессе
                stackcollapse.stdout.close()
                
                # Ждем завершения
                flamegraph.communicate()
        
        # Очищаем временный файл
        if os.path.exists(PERF_SCRIPT_OUTPUT):
            os.remove(PERF_SCRIPT_OUTPUT)
        
        print(f'Flamegraph saved to {FLAMEGRAPH_SVG}')
        
        # Проверяем размер результирующего файла
        if os.path.exists(FLAMEGRAPH_SVG) and os.path.getsize(FLAMEGRAPH_SVG) > 0:
            with open(FLAMEGRAPH_SVG, 'r') as f:
                content = f.read()
                if 'ERROR: No valid input' in content:
                    print('Error: flamegraph.pl did not receive valid input')
                    return False
                if 'RequestHandler' in content:
                    print('Flamegraph contains RequestHandler methods')
                else:
                    print('Warning: Flamegraph does not contain RequestHandler methods')
            return True
        
        return False
        
    except Exception as e:
        print(f'Error building flamegraph: {e}')
        return False


def main():
    # Удаляем старые файлы, если они существуют
    for f in [PERF_DATA_FILE, PERF_SCRIPT_OUTPUT, FLAMEGRAPH_SVG]:
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
    
    # Расчет времени записи
    record_duration = SHOOT_COUNT * COOLDOWN + 5
    
    # Запускаем perf record в фоновом режиме
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
    
    # Ждем завершения perf record
    print('Waiting for perf record to finish...')
    stop(perf_process, wait=True)
    
    # Останавливаем сервер
    print('Stopping server...')
    stop(server)
    
    # Даем время на завершение
    time.sleep(1)
    
    # Проверяем, что perf.data создан и не пуст
    if os.path.exists(PERF_DATA_FILE) and os.path.getsize(PERF_DATA_FILE) > 0:
        print(f'Perf data collected successfully ({os.path.getsize(PERF_DATA_FILE)} bytes)')
        
        # Строим флеймграф
        print('Building flamegraph...')
        build_flamegraph()
    else:
        print('Error: perf.data not created or empty')
    
    print('Job done')


if __name__ == '__main__':
    main()
