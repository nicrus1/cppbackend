import argparse
import subprocess
import time
import random
import shlex
import signal
import os
import sys

RANDOM_LIMIT = 1000
SEED = 123456789
random.seed(SEED)

AMMUNITION = [
    'localhost:8080/api/v1/maps/map1',
    'localhost:8080/api/v1/maps'
]

SHOOT_COUNT = 2000  # Увеличено для лучшего профилирования
COOLDOWN = 0.0005   # 0.5ms между запросами

def start_server():
    parser = argparse.ArgumentParser()
    parser.add_argument('server', type=str)
    return parser.parse_args().server

def run(command, output=None, error=None):
    process = subprocess.Popen(shlex.split(command), stdout=output, stderr=error)
    return process

def stop(process, wait=False):
    if process.poll() is None and wait:
        process.wait()
    process.terminate()

def shoot(ammo):
    # Используем curl с таймаутом
    hit = run(f'curl -s --max-time 2 {ammo}', 
              output=subprocess.DEVNULL, 
              error=subprocess.DEVNULL)
    time.sleep(COOLDOWN)
    stop(hit, wait=True)

def make_shots():
    print(f"Starting {SHOOT_COUNT} shots...")
    for i in range(SHOOT_COUNT):
        ammo_number = random.randrange(RANDOM_LIMIT) % len(AMMUNITION)
        shoot(AMMUNITION[ammo_number])
        if i % 200 == 0:
            print(f'Progress: {i}/{SHOOT_COUNT} shots completed')
    print(f'Shooting complete: {SHOOT_COUNT} shots fired')

# Parse server command
server_command = start_server()
print(f"Starting server: {server_command}")

# ИСПРАВЛЕНО: Запускаем сервер и perf в одной команде с sudo
# Обратите внимание: sudo может запросить пароль, в CI-среде это обычно настроено.
server_command_with_perf = f'sudo perf record -F 999 -g -o perf.data -- {server_command}'

print(f"Starting server and perf: {server_command_with_perf}")

# Запускаем процесс сервера под управлением perf
server_process = subprocess.Popen(shlex.split(server_command_with_perf),
                                  stdout=subprocess.PIPE,
                                  stderr=subprocess.PIPE,
                                  universal_newlines=True,
                                  bufsize=1)

# Даем время на запуск (perf и серверу нужно немного больше времени)
print("Waiting for server to start...")
time.sleep(5)

# Проверяем, запустился ли сервер, через curl
print("Checking if server is ready...")
for i in range(10):
    try:
        check = subprocess.run(['curl', '-s', 'http://localhost:8080/api/v1/maps'], 
                             capture_output=True, timeout=2)
        if check.returncode == 0:
            print("Server is ready!")
            break
    except:
        pass
    if i == 9:
        print("Warning: Server may not be ready, but continuing...")
    time.sleep(1)

# Run the shooting
make_shots()

# Даем время на завершение последних запросов
time.sleep(2)

# После окончания стрельбы останавливаем сервер (и perf вместе с ним)
print("Stopping server and perf...")
server_process.send_signal(signal.SIGINT) # Шлем SIGINT, чтобы perf завершил запись

try:
    server_process.wait(timeout=10)
except subprocess.TimeoutExpired:
    print("Server didn't stop gracefully, forcing termination...")
    server_process.terminate()
    server_process.wait()

# Ждем, пока perf завершит запись данных
time.sleep(2)

print("Generating flamegraph...")

# ИСПРАВЛЕНО: Преобразуем perf.data в читаемый формат и сразу подаем на вход FlameGraph
if os.path.exists('perf.data') and os.path.getsize('perf.data') > 0:
    print(f"perf.data size: {os.path.getsize('perf.data')} bytes")
    
    # Путь к скриптам FlameGraph (предполагается, что они уже склонированы)
    flamegraph_dir = 'FlameGraph'
    
    # Проверяем наличие FlameGraph
    if not os.path.exists(flamegraph_dir):
        print(f"FlameGraph directory not found at {flamegraph_dir}, trying to find it...")
        # Поиск в возможных местах
        possible_paths = [
            '/usr/local/FlameGraph',
            '/opt/FlameGraph',
            os.path.join(os.path.dirname(os.path.abspath(__file__)), 'FlameGraph'),
            os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'FlameGraph')
        ]
        for path in possible_paths:
            if os.path.exists(path):
                flamegraph_dir = path
                print(f"Found FlameGraph at {flamegraph_dir}")
                break
    
    if os.path.exists(flamegraph_dir):
        stackcollapse_script = os.path.join(flamegraph_dir, 'stackcollapse-perf.pl')
        flamegraph_script = os.path.join(flamegraph_dir, 'flamegraph.pl')
        
        # Проверяем наличие необходимых скриптов
        if os.path.exists(stackcollapse_script) and os.path.exists(flamegraph_script):
            print("Creating pipeline: perf script | stackcollapse-perf.pl | flamegraph.pl > graph.svg")
            
            # Создаем пайплайн: perf script | stackcollapse-perf.pl | flamegraph.pl > graph.svg
            try:
                with open('graph.svg', 'w') as svg_file:
                    # Запускаем perf script
                    perf_script = subprocess.Popen(['perf', 'script'], 
                                                  stdout=subprocess.PIPE, 
                                                  stderr=subprocess.DEVNULL)
                    
                    # Запускаем stackcollapse-perf.pl
                    stackcollapse = subprocess.Popen(['perl', stackcollapse_script], 
                                                    stdin=perf_script.stdout, 
                                                    stdout=subprocess.PIPE, 
                                                    stderr=subprocess.DEVNULL)
                    
                    # Закрываем ненужные дескрипторы в родительском процессе
                    perf_script.stdout.close()
                    
                    # Запускаем flamegraph.pl
                    flamegraph = subprocess.Popen(['perl', flamegraph_script], 
                                                 stdin=stackcollapse.stdout, 
                                                 stdout=svg_file, 
                                                 stderr=subprocess.PIPE)
                    
                    # Закрываем ненужные дескрипторы
                    stackcollapse.stdout.close()
                    
                    # Ждем завершения flamegraph
                    flamegraph.wait(timeout=30)
                    
                    # Проверяем, не было ли ошибок
                    if flamegraph.returncode != 0:
                        stderr = flamegraph.stderr.read().decode() if flamegraph.stderr else ''
                        print(f"Flamegraph error: {stderr}")
                    
                    # Ждем завершения остальных процессов
                    perf_script.wait(timeout=10)
                    stackcollapse.wait(timeout=10)
                
                # Проверяем результат
                if os.path.exists('graph.svg') and os.path.getsize('graph.svg') > 1000:
                    print(f"Flamegraph generated successfully: graph.svg ({os.path.getsize('graph.svg')} bytes)")
                    
                    # Проверяем содержимое
                    with open('graph.svg', 'r') as f:
                        content = f.read()
                        if 'http_handler' in content or 'RequestHandler' in content:
                            print("✓ Verified: http_handler/RequestHandler found in flamegraph")
                        else:
                            print("⚠ Warning: Expected symbols not found in flamegraph")
                else:
                    print(f"Failed to generate graph.svg or file too small (size: {os.path.getsize('graph.svg') if os.path.exists('graph.svg') else '0'})")
                    
            except subprocess.TimeoutExpired:
                print("Timeout while generating flamegraph")
            except Exception as e:
                print(f"Error in flamegraph generation: {e}")
        else:
            print(f"FlameGraph scripts not found: {stackcollapse_script} or {flamegraph_script}")
            print("Attempting to clone FlameGraph repository...")
            try:
                subprocess.run(['git', 'clone', 'https://github.com/brendangregg/FlameGraph.git'], 
                             check=False, capture_output=True)
                if os.path.exists('FlameGraph'):
                    print("FlameGraph cloned successfully, please re-run the script")
                else:
                    print("Failed to clone FlameGraph")
            except Exception as e:
                print(f"Error cloning FlameGraph: {e}")
    else:
        print(f"FlameGraph directory not found at {flamegraph_dir}")
else:
    print(f"perf.data not found or empty (size: {os.path.getsize('perf.data') if os.path.exists('perf.data') else 'does not exist'})")

print('Job completed')