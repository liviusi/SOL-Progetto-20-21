#!/bin/bash

GREEN="\033[0;32m"
RESET_COLOR="\033[0m"

# starting
echo -e "${GREEN}TEST 1${RESET_COLOR}"
echo -e "${GREEN}\tWARNING: THIS TEST TAKES A LONG TIME${RESET_COLOR}"
# creating dummies
echo -e "${GREEN}Creating dummy files (this may take a while...)${RESET_COLOR}"
mkdir dummies
touch dummies/1.txt
touch dummies/2.txt
touch dummies/3.txt
touch dummies/4.txt
touch dummies/5.txt
touch dummies/6.txt
touch dummies/7.txt
touch dummies/7.txt
touch dummies/8.txt
touch dummies/9.txt
head -c 10MB /dev/urandom | tr -dc 'a-zA-Z0-9~!@#$%^&*_-' | fold -w 50 > dummies/1.txt
head -c 20MB /dev/urandom | tr -dc 'a-zA-Z0-9~!@#$%^&*_-' | fold -w 50 > dummies/2.txt
head -c 30MB /dev/urandom | tr -dc 'a-zA-Z0-9~!@#$%^&*_-' | fold -w 50 > dummies/3.txt
head -c 40MB /dev/urandom | tr -dc 'a-zA-Z0-9~!@#$%^&*_-' | fold -w 50 > dummies/4.txt
head -c 50MB /dev/urandom | tr -dc 'a-zA-Z0-9~!@#$%^&*_-' | fold -w 50 > dummies/5.txt
head -c 60MB /dev/urandom | tr -dc 'a-zA-Z0-9~!@#$%^&*_-' | fold -w 50 > dummies/6.txt
head -c 70MB /dev/urandom | tr -dc 'a-zA-Z0-9~!@#$%^&*_-' | fold -w 50 > dummies/7.txt
head -c 75MB /dev/urandom | tr -dc 'a-zA-Z0-9~!@#$%^&*_-' | fold -w 50 > dummies/7.txt
head -c 80MB /dev/urandom | tr -dc 'a-zA-Z0-9~!@#$%^&*_-' | fold -w 50 > dummies/8.txt
head -c 90MB /dev/urandom | tr -dc 'a-zA-Z0-9~!@#$%^&*_-' | fold -w 50 > dummies/9.txt

# booting the server
echo -e "${GREEN}[FIFO] Booting the server${RESET_COLOR}"
valgrind --leak-check=full build/server ./config1.txt &
# server pid
SERVER_PID=$!
export SERVER_PID

sleep 5s

server_file="$(pwd)/src/server.c"
config_file="$(pwd)/src/config.c"

echo -e "${GREEN}[FIFO] Running some clients${RESET_COLOR}"
# print help message
build/client -h
# connect
build/client -p -t 200 -f socket.sk
# send directory, store victims if any
build/client -p -t 200 -f socket.sk -w dummies -D test1/FIFO/victims
# send directory, store victims if any
build/client -p -t 200 -f socket.sk -w src -D test1/FIFO/victims
# read every file inside server, send to directory, unlock a file
build/client -p -t 200 -f socket.sk -R 0 -d test1/FIFO/read-files -u ${config_file}
# write a file inside storage, lock it, delete a file
build/client -p -t 200 -f socket.sk -W ${server_file} -l ${server_file} -r ${config_file},${server_file} -c ${server_file}

echo -e "${GREEN}[FIFO] Terminating server with SIGHUP${RESET_COLOR}"
kill -s SIGHUP $SERVER_PID
wait $SERVER_PID
echo -e "${GREEN}[FIFO] Test completed${RESET_COLOR}"

# replace 0 with 1 in config1.txt => use LRU replacement policy
sed -i '$s/0/1/' config1.txt
# change log file name
sed -i -e 's/FIFO1.log/LRU1.log/g' config1.txt

echo -e "${GREEN}[LRU] Booting the server${RESET_COLOR}"

valgrind --leak-check=full build/server ./config1.txt &
# server pid
SERVER_PID=$!
export SERVER_PID

sleep 5s

echo -e "${GREEN}[LRU] Running some clients${RESET_COLOR}"
# print help message
build/client -h
# connect
build/client -p -t 200 -f socket.sk
# send directory, store victims if any
build/client -p -t 200 -f socket.sk -w dummies -D test1/LRU/victims
# send directory, store victims if any
build/client -p -t 200 -f socket.sk -w src -D test1/LRU/victims
# read every file inside server, send to directory, unlock a file
build/client -p -t 200 -f socket.sk -R 0 -d test1/LRU/read-files -u ${config_file}
# write a file inside storage, lock it, delete a file
build/client -p -t 200 -f socket.sk -W ${server_file} -l ${server_file} -r ${config_file},${server_file} -c ${server_file}

echo -e "${GREEN}[LRU] Terminating server with SIGHUP${RESET_COLOR}"
kill -s SIGHUP $SERVER_PID
wait $SERVER_PID
echo -e "${GREEN}[LRU] Test completed${RESET_COLOR}"

# replace 1 with 2 in config1.txt => use LFU replacement policy
sed -i '$s/1/2/' config1.txt
# change log file name
sed -i -e 's/LRU1.log/LFU1.log/g' config1.txt

echo -e "${GREEN}[LFU] Booting the server${RESET_COLOR}"

valgrind --leak-check=full build/server ./config1.txt &
# server pid
SERVER_PID=$!
export SERVER_PID

sleep 5s

echo -e "${GREEN}[LFU] Running some clients${RESET_COLOR}"
# print help message
build/client -h
# connect
build/client -p -t 200 -f socket.sk
# send directory, store victims if any
build/client -p -t 200 -f socket.sk -w dummies -D test1/LFU/victims
# send directory, store victims if any
build/client -p -t 200 -f socket.sk -w src -D test1/LFU/victims
# read every file inside server, send to directory, unlock a file
build/client -p -t 200 -f socket.sk -R 0 -d test1/LFU/read-files -u ${config_file}
# write a file inside storage, lock it, delete a file
build/client -p -t 200 -f socket.sk -W ${server_file} -l ${server_file} -r ${config_file},${server_file} -c ${server_file}

echo -e "${GREEN}[LFU] Terminating server with SIGHUP${RESET_COLOR}"
kill -s SIGHUP $SERVER_PID
wait $SERVER_PID
echo -e "${GREEN}[LFU] Test completed${RESET_COLOR}"
echo -e "${GREEN}TEST COMPLETED${RESET_COLOR}"
rm config1.txt
rm -r dummies
exit 0