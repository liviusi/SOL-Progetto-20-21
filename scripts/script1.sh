#!/bin/bash

GREEN="\033[0;32m"
RESET_COLOR="\033[0m"
# starting
echo -e "${GREEN}Test 1${RESET_COLOR}"
# booting the server
valgrind --leak-check=full build/server ./config1.txt &
# server pid
SERVER_PID=$!

sleep 5s

server_file="$(pwd)/src/server.c"
config_file="$(pwd)/src/config.c"

build/client -h
build/client -p -t 200 -f socket.sk
build/client -p -t 200 -f socket.sk -w src -D victims
build/client -p -t 200 -f socket.sk -R 0 -d read-files -u ${config_file}
build/client -p -t 200 -f socket.sk -W ${server_file} -l ${server_file} -c ${server_file}

sleep 5s

kill -s SIGHUP $pid
wait $pid

sleep 5s
rm config1.txt