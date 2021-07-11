#!/bin/bash

GREEN="\033[0;32m"
RESET_COLOR="\033[0m"
# starting
echo -e "${GREEN}TEST 2${RESET_COLOR}"

echo -e "${GREEN}Creating dummy files${RESET_COLOR}"
mkdir dummies1
touch dummies1/1.txt
touch dummies1/2.txt
touch dummies1/3.txt
touch dummies1/4.txt
touch dummies1/5.txt
touch dummies1/6.txt
touch dummies1/7.txt
touch dummies1/8.txt
touch dummies1/9.txt
touch dummies1/10.txt
head -c 300KB /dev/urandom | tr -dc 'a-zA-Z0-9~!@#$%^&*_-' | fold -w 50 > dummies1/1.txt
head -c 350KB /dev/urandom | tr -dc 'a-zA-Z0-9~!@#$%^&*_-' | fold -w 50 > dummies1/2.txt
head -c 400KB /dev/urandom | tr -dc 'a-zA-Z0-9~!@#$%^&*_-' | fold -w 50 > dummies1/3.txt
head -c 500KB /dev/urandom | tr -dc 'a-zA-Z0-9~!@#$%^&*_-' | fold -w 50 > dummies1/4.txt
head -c 600KB /dev/urandom | tr -dc 'a-zA-Z0-9~!@#$%^&*_-' | fold -w 50 > dummies1/5.txt
head -c 700KB /dev/urandom | tr -dc 'a-zA-Z0-9~!@#$%^&*_-' | fold -w 50 > dummies1/6.txt
head -c 800KB /dev/urandom | tr -dc 'a-zA-Z0-9~!@#$%^&*_-' | fold -w 50 > dummies1/7.txt
head -c 900KB /dev/urandom | tr -dc 'a-zA-Z0-9~!@#$%^&*_-' | fold -w 50 > dummies1/8.txt
head -c 1MB /dev/urandom | tr -dc 'a-zA-Z0-9~!@#$%^&*_-' | fold -w 50 > dummies1/9.txt
head -c 1MB /dev/urandom | tr -dc 'a-zA-Z0-9~!@#$%^&*_-' | fold -w 50 > dummies1/10.txt
cp -r dummies1 dummies2
cp -r dummies2 dummies3

# booting the server
echo -e "${GREEN}[FIFO] Booting the server${RESET_COLOR}"
build/server ./config2.txt &
# server pid
SERVER_PID=$!
export SERVER_PID
sleep 5s

echo -e "${GREEN}[FIFO] Running some clients${RESET_COLOR}"
# connect, send files, save victims if any
build/client -p -t 10 -f socket.sk -w dummies1 -D test2/FIFO/victims1
# connect, send files, save victims if any
build/client -p -t 10 -f socket.sk -w dummies2 -D test2/FIFO/victims2
# connect, send files, save victims if any
build/client -p -t 10 -f socket.sk -w dummies3 -D test2/FIFO/victims3

echo -e "${GREEN}[FIFO] Terminating server with SIGHUP${RESET_COLOR}"
kill -s SIGHUP $SERVER_PID
wait $SERVER_PID
echo -e "${GREEN}[FIFO] Test completed${RESET_COLOR}"

# replace 0 with 1 in config2.txt => use LRU replacement policy
sed -i '$s/0/1/' config2.txt
# change log file name
sed -i -e 's/FIFO2.log/LRU2.log/g' config2.txt

echo -e "${GREEN}[LRU] Booting the server${RESET_COLOR}"

build/server ./config2.txt &
# server pid
SERVER_PID=$!
export SERVER_PID

sleep 5s

echo -e "${GREEN}[LRU] Running some clients${RESET_COLOR}"
# connect, send files, save victims if any
build/client -p -t 10 -f socket.sk -w dummies1 -D test2/LRU/victims1
# connect, send files, save victims if any
build/client -p -t 10 -f socket.sk -w dummies2 -D test2/LRU/victims2
# connect, send files, save victims if any
build/client -p -t 10 -f socket.sk -w dummies3 -D test2/LRU/victims3

echo -e "${GREEN}[LRU] Terminating server with SIGHUP${RESET_COLOR}"
kill -s SIGHUP $SERVER_PID
wait $SERVER_PID
echo -e "${GREEN}[LRU] Test completed${RESET_COLOR}"

# replace 1 with 2 in config2.txt => use LFU replacement policy
sed -i '$s/1/2/' config2.txt
# change log file name
sed -i -e 's/LRU2.log/LFU2.log/g' config2.txt

echo -e "${GREEN}[LFU] Booting the server${RESET_COLOR}"

build/server ./config2.txt &
# server pid
SERVER_PID=$!
export SERVER_PID

sleep 5s

echo -e "${GREEN}[LFU] Running some clients${RESET_COLOR}"
# connect, send files, save victims if any
build/client -p -t 10 -f socket.sk -w dummies1 -D test2/LFU/victims1
# connect, send files, save victims if any
build/client -p -t 10 -f socket.sk -w dummies2 -D test2/LFU/victims2
# connect, send files, save victims if any
build/client -p -t 10 -f socket.sk -w dummies3 -D test2/LFU/victims3

echo -e "${GREEN}[LFU] Terminating server with SIGHUP${RESET_COLOR}"
kill -s SIGHUP $SERVER_PID
wait $SERVER_PID
echo -e "${GREEN}[LFU] Test completed${RESET_COLOR}"
echo -e "${GREEN}TEST COMPLETED${RESET_COLOR}"

rm config2.txt
rm -r dummies1 dummies2 dummies3
exit 0