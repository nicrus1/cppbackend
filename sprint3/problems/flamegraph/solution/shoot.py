import argparse
import subprocess
import time
import random
import shlex
import signal
import os

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
    process = subprocess.Popen(shlex.split(command), stdout=output, stderr=subprocess.DEVNULL)
    return process


def stop(process, wait=False):
    if process.poll() is None and wait:
        process.wait()
    process.terminate()


def shoot(ammo):
    hit = run('curl ' + ammo, output=subprocess.DEVNULL)
    time.sleep(COOLDOWN)
    stop(hit, wait=True)


def make_shots():
    for _ in range(SHOOT_COUNT):
        ammo_number = random.randrange(RANDOM_LIMIT) % len(AMMUNITION)
        shoot(AMMUNITION[ammo_number])
    print('Shooting complete')


# Start the server
server_command = start_server()
server_process = run(server_command)

# Give the server time to start up
time.sleep(1)

# Start perf record for the server process
# Get the PID of the server process (assuming it's the first child or main process)
# Since the server might spawn multiple processes, we need the main process ID
server_pid = server_process.pid

# Start perf record with the server PID
perf_process = run(f'perf record -F 99 -g -p {server_pid} -o perf.data', output=subprocess.DEVNULL)

# Wait a moment for perf to start recording
time.sleep(0.5)

# Run the shooting
make_shots()

# Stop the perf recording
# First, send SIGINT to perf to stop recording gracefully
perf_process.send_signal(signal.SIGINT)
time.sleep(1)

# If perf hasn't terminated, terminate it
if perf_process.poll() is None:
    stop(perf_process, wait=True)
else:
    perf_process.wait()

# Now stop the server
stop(server_process)
time.sleep(1)

# Generate flamegraph if perf.data exists and is valid
if os.path.exists('perf.data') and os.path.getsize('perf.data') > 0:
    # Create the flamegraph using the double pipe
    try:
        # First, convert perf.data to text format with perf script
        # Then collapse the stacks and generate the SVG
        with open('perf.script', 'w') as script_output:
            perf_script = subprocess.Popen(['perf', 'script', '-i', 'perf.data'], 
                                          stdout=script_output, 
                                          stderr=subprocess.DEVNULL)
            perf_script.wait()
        
        # Check if FlameGraph directory exists
        if os.path.exists('FlameGraph'):
            # Run stackcollapse-perf.pl and flamegraph.pl
            with open('perf.script', 'r') as script_input:
                stackcollapse = subprocess.Popen(['perl', 'FlameGraph/stackcollapse-perf.pl'], 
                                                stdin=script_input,
                                                stdout=subprocess.PIPE,
                                                stderr=subprocess.DEVNULL)
                
                with open('graph.svg', 'w') as svg_output:
                    flamegraph = subprocess.Popen(['perl', 'FlameGraph/flamegraph.pl'], 
                                                 stdin=stackcollapse.stdout,
                                                 stdout=svg_output,
                                                 stderr=subprocess.DEVNULL)
                    flamegraph.wait()
                    stackcollapse.wait()
            
            print('Flamegraph generated: graph.svg')
        else:
            print('Warning: FlameGraph directory not found')
    except Exception as e:
        print(f'Error generating flamegraph: {e}')
else:
    print('Error: perf.data not found or empty')

print('Job done')