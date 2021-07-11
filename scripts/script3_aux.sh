#!/bin/bash
while true
do
	printf -v tmp '%(%s)T' -1 # time elapsed since epoch
	build/client -f socket.sk -w dummies$((1 + $tmp % 10))
done