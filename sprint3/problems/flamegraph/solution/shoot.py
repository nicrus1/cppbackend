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

# Start the server process
server_process = subprocess.Popen(shlex.split(server_command), 
                                  stdout=subprocess.PIPE, 
                                  stderr=subprocess.PIPE,
                                  universal_newlines=True,
                                  bufsize=1)

# Give the server time to start up
print("Waiting for server to start...")
time.sleep(3)

# Get the server PID
server_pid = server_process.pid
print(f"Server PID: {server_pid}")

# Улучшенная команда perf для лучшего сбора данных
perf_cmd = f'perf record -F 999 -g --call-graph dwarf -p {server_pid} -o perf.data -- sleep 35'
print(f"Running: {perf_cmd}")

# Start perf record in the background
perf_process = subprocess.Popen(shlex.split(perf_cmd),
                                stdout=subprocess.DEVNULL,
                                stderr=subprocess.PIPE)

# Wait a moment for perf to start recording
time.sleep(2)

# Run the shooting
make_shots()

# Give a moment for any pending requests to complete
time.sleep(1)

# Stop perf recording
print("Stopping perf recording...")
perf_process.send_signal(signal.SIGINT)

# Wait for perf to terminate
try:
    perf_process.wait(timeout=10)
except subprocess.TimeoutExpired:
    print("Perf didn't stop gracefully, terminating...")
    perf_process.terminate()
    perf_process.wait()

# Stop the server
print("Stopping server...")
stop(server_process)
server_process.wait()

# Wait for cleanup
time.sleep(2)

# Generate flamegraph if perf.data exists and is non-empty
print("Generating flamegraph...")

if os.path.exists('perf.data') and os.path.getsize('perf.data') > 0:
    print(f"perf.data size: {os.path.getsize('perf.data')} bytes")
    
    # Check if FlameGraph directory exists
    flamegraph_dir = 'FlameGraph'
    if not os.path.exists(flamegraph_dir):
        # Try to find it in the parent directory
        script_dir = os.path.dirname(os.path.abspath(__file__))
        possible_paths = [
            os.path.join(script_dir, 'FlameGraph'),
            os.path.join(os.path.dirname(script_dir), 'FlameGraph'),
            '/usr/local/FlameGraph',
            '/opt/FlameGraph'
        ]
        for path in possible_paths:
            if os.path.exists(path):
                flamegraph_dir = path
                break
    
    if os.path.exists(flamegraph_dir):
        stackcollapse_script = os.path.join(flamegraph_dir, 'stackcollapse-perf.pl')
        flamegraph_script = os.path.join(flamegraph_dir, 'flamegraph.pl')
        
        if os.path.exists(stackcollapse_script) and os.path.exists(flamegraph_script):
            print("Generating flamegraph from perf data...")
            
            try:
                # Run perf script and pipe to flamegraph tools
                with open('perf.script', 'w') as f:
                    subprocess.run(['perf', 'script', '-i', 'perf.data'], 
                                 stdout=f, 
                                 stderr=subprocess.DEVNULL,
                                 check=False)
                
                # Generate collapsed stacks
                with open('perf.script', 'r') as infile, open('stacks.collapsed', 'w') as outfile:
                    subprocess.run(['perl', stackcollapse_script], 
                                 stdin=infile, 
                                 stdout=outfile,
                                 stderr=subprocess.DEVNULL,
                                 check=False)
                
                # Generate flamegraph
                with open('stacks.collapsed', 'r') as infile, open('graph.svg', 'w') as outfile:
                    subprocess.run(['perl', flamegraph_script], 
                                 stdin=infile, 
                                 stdout=outfile,
                                 stderr=subprocess.PIPE,
                                 check=False)
                
                # Cleanup intermediate files
                for f in ['perf.script', 'stacks.collapsed']:
                    if os.path.exists(f):
                        os.remove(f)
                
                if os.path.exists('graph.svg') and os.path.getsize('graph.svg') > 1000:
                    print(f"Flamegraph generated successfully: graph.svg ({os.path.getsize('graph.svg')} bytes)")
                    
                    # Verify content
                    with open('graph.svg', 'r') as f:
                        content = f.read()
                        if 'http_handler' in content:
                            print("✓ Verified: http_handler functions found in flamegraph")
                        elif 'RequestHandler' in content:
                            print("✓ Verified: RequestHandler found in flamegraph")
                        else:
                            print("⚠ Warning: Expected symbols not found in flamegraph")
                else:
                    print("Failed to generate graph.svg or file too small")
                    
            except Exception as e:
                print(f"Error in flamegraph generation: {e}")
        else:
            print(f"FlameGraph scripts not found: {stackcollapse_script} or {flamegraph_script}")
    else:
        print(f"FlameGraph directory not found at {flamegraph_dir}")
        
        # Try to clone FlameGraph if not exists
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
    print(f"perf.data not found or empty (size: {os.path.getsize('perf.data') if os.path.exists('perf.data') else 'does not exist'})")

print('Job completed')