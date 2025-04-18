#!/bin/bash

# Server details
HOST="127.0.0.1"  # Server IP address (localhost)
PORT=4242         # Server port (replace with your server's port)

# Number of clients to create
CLIENTS=100

# Delay between launching each client (in seconds)
DELAY=0.1

# Array to hold PIDs of nc processes
pids=()

echo "Starting $CLIENTS clients..."

# Launch 100 clients and store their PIDs
for i in $(seq 1 $CLIENTS); do
    nc $HOST $PORT &
    pids+=($!)  # Store the PID of the background process
    echo "Client $i launched with PID $!"
    sleep $DELAY
done

echo "All clients launched."

# Prompt user to close clients and wait for Enter
read -p "close clients? (hit Enter to close)"

echo "Closing clients..."

# Close all 100 clients by killing their processes
for pid in "${pids[@]}"; do
    sleep $DELAY
    kill $pid  # Send SIGTERM to terminate the process
    wait $pid 2>/dev/null  # Wait for the process to finish, suppress errors
done

echo "All clients closed."