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


def run(command, output=None, error=None):
    process = subprocess.Popen(shlex.split(command), stdout=output, stderr=error)
    return process


def stop(process, wait=False):
    if process.poll() is None and wait:
        process.wait()
    process.terminate()


def shoot(ammo):
    hit = run('curl -s ' + ammo, output=subprocess.DEVNULL, error=subprocess.DEVNULL)
    time.sleep(COOLDOWN)
    stop(hit, wait=True)


def make_shots():
    for _ in range(SHOOT_COUNT):
        ammo_number = random.randrange(RANDOM_LIMIT) % len(AMMUNITION)
        shoot(AMMUNITION[ammo_number])
    print('Shooting complete')


# Parse server command
server_command = start_server()
print(f"Starting server: {server_command}")

# Start the server process with pipes to capture output
server_process = subprocess.Popen(shlex.split(server_command), 
                                  stdout=subprocess.PIPE, 
                                  stderr=subprocess.PIPE)

# Give the server time to start up
time.sleep(2)

# Get the server PID
server_pid = server_process.pid
print(f"Server PID: {server_pid}")

# Start perf record - we need to use sudo for proper profiling
# But since we might not have sudo, let's try without first
perf_cmd = f'perf record -F 99 -g -p {server_pid} -o perf.data -- sleep 30'
print(f"Running: {perf_cmd}")

# Start perf record in the background
# We'll run it with a timeout to automatically stop after 30 seconds
perf_process = subprocess.Popen(shlex.split(perf_cmd),
                                stdout=subprocess.DEVNULL,
                                stderr=subprocess.PIPE)

# Wait a moment for perf to start recording
time.sleep(1)

# Run the shooting - this will take about 10 seconds (100 * 0.1)
make_shots()

# Wait for perf to finish (it will stop after 30 seconds anyway)
# But let's send SIGINT after shooting is done to stop it earlier
print("Stopping perf recording...")
perf_process.send_signal(signal.SIGINT)

# Wait for perf to terminate
try:
    perf_process.wait(timeout=5)
except subprocess.TimeoutExpired:
    perf_process.terminate()
    perf_process.wait()

# Stop the server
print("Stopping server...")
stop(server_process)
server_process.wait()

# Wait for cleanup
time.sleep(1)

# Generate flamegraph if perf.data exists and is non-empty
print("Generating flamegraph...")

if os.path.exists('perf.data') and os.path.getsize('perf.data') > 0:
    print(f"perf.data size: {os.path.getsize('perf.data')} bytes")
    
    # Check if FlameGraph directory exists
    flamegraph_dir = 'FlameGraph'
    if not os.path.exists(flamegraph_dir):
        script_dir = os.path.dirname(os.path.abspath(__file__))
        flamegraph_dir = os.path.join(script_dir, 'FlameGraph')
    
    if os.path.exists(flamegraph_dir):
        stackcollapse_script = os.path.join(flamegraph_dir, 'stackcollapse-perf.pl')
        flamegraph_script = os.path.join(flamegraph_dir, 'flamegraph.pl')
        
        # First, check if perf script can read the data
        check_result = subprocess.run(['perf', 'script', '-i', 'perf.data', '--header', '-D'],
                                     capture_output=True, text=True)
        
        if check_result.returncode != 0:
            print(f"Error reading perf.data: {check_result.stderr}")
        else:
            print("perf.data is valid, generating flamegraph...")
            
            # Create the pipeline: perf script | stackcollapse-perf.pl | flamegraph.pl
            try:
                # Use subprocess.PIPE to connect the processes
                perf_script = subprocess.Popen(['perf', 'script', '-i', 'perf.data'],
                                              stdout=subprocess.PIPE,
                                              stderr=subprocess.DEVNULL)
                
                stackcollapse = subprocess.Popen(['perl', stackcollapse_script],
                                                stdin=perf_script.stdout,
                                                stdout=subprocess.PIPE,
                                                stderr=subprocess.DEVNULL)
                
                # Close perf_script stdout in the parent process
                perf_script.stdout.close()
                
                with open('graph.svg', 'w') as svg_file:
                    flamegraph = subprocess.Popen(['perl', flamegraph_script],
                                                 stdin=stackcollapse.stdout,
                                                 stdout=svg_file,
                                                 stderr=subprocess.PIPE)
                    
                    # Close stackcollapse stdout in the parent process
                    stackcollapse.stdout.close()
                    
                    # Wait for flamegraph to complete
                    flamegraph.wait()
                    
                    # Check if flamegraph succeeded
                    if flamegraph.returncode != 0:
                        stderr = flamegraph.stderr.read().decode() if flamegraph.stderr else ''
                        print(f"Flamegraph error: {stderr}")
                
                # Wait for the other processes
                perf_script.wait()
                stackcollapse.wait()
                
                if os.path.exists('graph.svg') and os.path.getsize('graph.svg') > 0:
                    print(f"Flamegraph generated: graph.svg ({os.path.getsize('graph.svg')} bytes)")
                    
                    # Verify content
                    with open('graph.svg', 'r') as f:
                        content = f.read()
                        if 'ERROR' in content:
                            print("Error in flamegraph generation")
                        elif 'http_handler::RequestHandler' in content:
                            print("Verified: RequestHandler found in flamegraph")
                        else:
                            print("Warning: RequestHandler not found in flamegraph")
                else:
                    print("Failed to generate graph.svg")
                    
            except Exception as e:
                print(f"Error in pipeline: {e}")
    else:
        print(f"FlameGraph directory not found at {flamegraph_dir}")
else:
    print(f"perf.data not found or empty (size: {os.path.getsize('perf.data') if os.path.exists('perf.data') else 'does not exist'})")

print('Job done')