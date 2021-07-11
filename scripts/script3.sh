#!/bin/bash

GREEN="\033[0;32m"
RESET_COLOR="\033[0m"
# starting
echo -e "${GREEN}TEST 3${RESET_COLOR}"

echo -e "${GREEN}Creating dummy files (this may take a while...)${RESET_COLOR}"
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
head -c 1MB /dev/urandom | tr -dc 'a-zA-Z0-9~!@#$%^&*_-' | fold -w 50 > dummies1/1.txt
head -c 5MB /dev/urandom | tr -dc 'a-zA-Z0-9~!@#$%^&*_-' | fold -w 50 > dummies1/2.txt
head -c 10MB /dev/urandom | tr -dc 'a-zA-Z0-9~!@#$%^&*_-' | fold -w 50 > dummies1/3.txt
head -c 15MB /dev/urandom | tr -dc 'a-zA-Z0-9~!@#$%^&*_-' | fold -w 50 > dummies1/4.txt
head -c 20MB /dev/urandom | tr -dc 'a-zA-Z0-9~!@#$%^&*_-' | fold -w 50 > dummies1/5.txt
head -c 25MB /dev/urandom | tr -dc 'a-zA-Z0-9~!@#$%^&*_-' | fold -w 50 > dummies1/6.txt
head -c 28MB /dev/urandom | tr -dc 'a-zA-Z0-9~!@#$%^&*_-' | fold -w 50 > dummies1/7.txt
head -c 29MB /dev/urandom | tr -dc 'a-zA-Z0-9~!@#$%^&*_-' | fold -w 50 > dummies1/8.txt
head -c 30MB /dev/urandom | tr -dc 'a-zA-Z0-9~!@#$%^&*_-' | fold -w 50 > dummies1/9.txt
head -c 31MB /dev/urandom | tr -dc 'a-zA-Z0-9~!@#$%^&*_-' | fold -w 50 > dummies1/10.txt
cp -r dummies1 dummies2
cp -r dummies1 dummies3
cp -r dummies1 dummies4
cp -r dummies1 dummies5
cp -r dummies1 dummies6
cp -r dummies1 dummies7
cp -r dummies1 dummies8
cp -r dummies1 dummies9
cp -r dummies1 dummies10

# booting the server
echo -e "${GREEN}[FIFO] Booting the server${RESET_COLOR}"
build/server ./config3.txt &
# server pid
SERVER_PID=$!
export SERVER_PID
sleep 5s

bash -c 'sleep 30 && kill -2 ${SERVER_PID}' &
echo -e "${GREEN}[FIFO] Terminating server with SIGINT in 30s${RESET_COLOR}"

# start 10 times aux script
aux_pids=()
for i in {1..10}; do
	bash -c 'scripts/script3_aux.sh' &
	aux_pids+=($!)
	sleep 0.1
done

sleep 30s

# kill spawned instances
for i in "${aux_pids[@]}"; do
	kill ${i}
	wait ${i} 2>/dev/null
done

wait $SERVER_PID
killall -q build/client # kill every running client
echo -e "${GREEN}[FIFO] Test completed${RESET_COLOR}"

# replace 0 with 1 in config3.txt => use LRU replacement policy
sed -i '$s/0/1/' config3.txt
# change log file name
sed -i -e 's/FIFO3.log/LRU3.log/g' config3.txt

echo -e "${GREEN}[LRU] Booting the server${RESET_COLOR}"
build/server ./config3.txt &
# server pid
SERVER_PID=$!
export SERVER_PID
sleep 5s

bash -c 'sleep 30 && kill -2 ${SERVER_PID}' &
echo -e "${GREEN}[LRU] Terminating server with SIGINT in 30s${RESET_COLOR}"

# start 10 times aux script
aux_pids=()
for i in {1..10}; do
	bash -c 'scripts/script3_aux.sh' &
	aux_pids+=($!)
	sleep 0.1
done

sleep 30s

# kill spawned instances
for i in "${aux_pids[@]}"; do
	kill ${i}
	wait ${i} 2>/dev/null
done

wait $SERVER_PID
killall -q build/client # kill every running client
echo -e "${GREEN}[LRU] Test completed${RESET_COLOR}"

# replace 1 with 2 in config3.txt => use LFU replacement policy
sed -i '$s/1/2/' config3.txt
# change log file name
sed -i -e 's/LRU3.log/LFU3.log/g' config3.txt

echo -e "${GREEN}[LFU] Booting the server${RESET_COLOR}"

build/server ./config3.txt &
# server pid
SERVER_PID=$!
export SERVER_PID
sleep 5s

bash -c 'sleep 30 && kill -2 ${SERVER_PID}' &
echo -e "${GREEN}[LFU] Terminating server with SIGINT in 30s${RESET_COLOR}"

# start 10 times aux script
aux_pids=()
for i in {1..10}; do
	bash -c 'scripts/script3_aux.sh' &
	aux_pids+=($!)
	sleep 0.1
done

sleep 30s

# kill spawned instances
for i in "${aux_pids[@]}"; do
	kill ${i}
	wait ${i} 2>/dev/null
done

wait $SERVER_PID
killall -q build/client # kill every running client
echo -e "${GREEN}[LFU] Test completed${RESET_COLOR}"

rm -rf dummies* config3.txt

echo -e "${GREEN}TEST COMPLETED${RESET_COLOR}"

exit 0