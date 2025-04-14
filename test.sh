#!/bin/bash

PORT=4242
SERVER=./mini_serv

# 🔥 Start the server in the background
$SERVER $PORT &
SERVER_PID=$!
echo "Server running with PID $SERVER_PID on port $PORT"

# 🧼 Cleanup trap
cleanup() {
	echo "Killing server (PID $SERVER_PID)..."
	kill $SERVER_PID 2>/dev/null || true
	killall nc 2>/dev/null || true
	exit
}
trap cleanup INT TERM

sleep 1

# 📡 Connect 3 clients via `nc`
for i in 1 2 3 4; do
	nc 127.0.0.1 $PORT > client_$i.log &
	eval CLIENT${i}_PID=$!
	echo "Connected client $i (PID $(eval echo \$CLIENT${i}_PID))"
	sleep 0.5
done

# ✏️ Send messages from clients
sleep 1
echo "Hello from client $i here here" | nc 127.0.0.1 $PORT &
sleep 0.5
echo -e "Multi-line\nMessage from client $i there there" | nc 127.0.0.1 $PORT &
sleep 0.5

# ⏳ Let things run for a bit
sleep 3

# 🔎 Show logs
for i in 1 2 3 4; do
	echo "------ client_$i.log ------"
	cat client_$i.log
done

# 💣 Kill everything
cleanup
