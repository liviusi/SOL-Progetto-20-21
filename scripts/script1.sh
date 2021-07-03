#!/bin/bash

GREEN="\033[0;32m"
RESET_COLOR="\033[0m"
# starting
echo -e "${GREEN}Test 1${RESET_COLOR}"
# creating dummies
echo -e "${GREEN}Creating dummy files${RESET_COLOR}"
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
echo -e "${GREEN}Booting the server${RESET_COLOR}"
valgrind --leak-check=full build/server ./config1.txt &
# server pid
SERVER_PID=$!
export SERVER_PID

sleep 5s

server_file="$(pwd)/src/server.c"
config_file="$(pwd)/src/config.c"

echo -e "${GREEN}Running some clients${RESET_COLOR}"
# print help message
build/client -h
# connect
build/client -p -t 200 -f socket.sk
# send directory, store victims if any inside ./victims
build/client -p -t 200 -f socket.sk -w dummies -D test1/victims
# send directory, store victims if any inside ./victims
build/client -p -t 200 -f socket.sk -w src -D test1/victims
# read every file inside server, send to directory ./read-files, unlock a file
build/client -p -t 200 -f socket.sk -R 0 -d test1/read-files -u ${config_file}
# write a file inside storage, lock it, delete a file
build/client -p -t 200 -f socket.sk -W ${server_file} -l ${server_file} -r ${config_file},${server_file} -c ${server_file}

echo -e "${GREEN}Terminating server with SIGHUP${RESET_COLOR}"
kill -s SIGHUP $SERVER_PID
wait $SERVER_PID
echo -e "${GREEN}Test completed${RESET_COLOR}"
rm config1.txt
rm -r dummies
exit 0