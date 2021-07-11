CC = gcc
CFLAGS += -std=c99 -Wall -g
INCLUDES = -I ./includes/
LIBS = -lpthread

BUILD_DIR := ./build
OBJ_DIR := ./obj
SRC_DIR := ./src
TEST_DIR := ./tests
HEADERS_DIR = ./includes/
DUMMIES_DIR = ./dummies

TARGETS = server client

.DEFAULT_GOAL := all

OBJS-SERVER = obj/node.o obj/linked_list.o obj/hashtable.o obj/rwlock.o obj/config.o obj/storage.o obj/bounded_buffer.o obj/server.o
OBJS-CLIENT = obj/node.o obj/linked_list.o obj/server_interface.o obj/client.o

obj/node.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c src/data_structures/node.c $(LIBS)
	@mv node.o $(OBJ_DIR)/node.o

obj/linked_list.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c src/data_structures/linked_list.c $(LIBS)
	@mv linked_list.o $(OBJ_DIR)/linked_list.o

obj/hashtable.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c src/data_structures/hashtable.c $(LIBS)
	@mv hashtable.o $(OBJ_DIR)/hashtable.o

obj/rwlock.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c src/data_structures/rwlock.c $(LIBS)
	@mv rwlock.o $(OBJ_DIR)/rwlock.o

obj/config.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c src/config.c $(LIBS)
	@mv config.o $(OBJ_DIR)/config.o

obj/storage.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c src/storage.c $(LIBS)
	@mv storage.o $(OBJ_DIR)/storage.o

obj/bounded_buffer.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c src/data_structures/bounded_buffer.c $(LIBS)
	@mv bounded_buffer.o $(OBJ_DIR)/bounded_buffer.o

obj/server.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c src/server.c $(LIBS)
	@mv server.o $(OBJ_DIR)/server.o

obj/server_interface.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c src/server_interface.c $(LIBS)
	@mv server_interface.o $(OBJ_DIR)/server_interface.o

obj/client.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c src/client.c $(LIBS)
	@mv client.o $(OBJ_DIR)/client.o

client: $(OBJS-CLIENT)
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/client $(OBJS-CLIENT) $(LIBS)

server: $(OBJS-SERVER)
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/server $(OBJS-SERVER) $(LIBS)

test1: client server
	@echo "NUMBER OF THREAD WORKERS = 1\nMAXIMUM NUMBER OF STORABLE FILES = 10000\nMAXIMUM STORAGE SIZE = 128000000\nSOCKET FILE PATH = $(PWD)/socket.sk\nLOG FILE PATH = $(PWD)/logs/FIFO1.log\nREPLACEMENT POLICY = 0" > config1.txt
	@chmod +x scripts/script1.sh
	scripts/script1.sh

test2: client server
	@echo "NUMBER OF THREAD WORKERS = 4\nMAXIMUM NUMBER OF STORABLE FILES = 10\nMAXIMUM STORAGE SIZE = 1000000\nSOCKET FILE PATH = $(PWD)/socket.sk\nLOG FILE PATH = $(PWD)/logs/FIFO2.log\nREPLACEMENT POLICY = 0" > config2.txt
	@chmod +x scripts/script2.sh
	scripts/script2.sh

test3: client server
	@echo "NUMBER OF THREAD WORKERS = 8\nMAXIMUM NUMBER OF STORABLE FILES = 100\nMAXIMUM STORAGE SIZE = 32000000\nSOCKET FILE PATH = $(PWD)/socket.sk\nLOG FILE PATH = $(PWD)/logs/FIFO3.log\nREPLACEMENT POLICY = 0" > config3.txt
	@chmod +x scripts/script3.sh
	@chmod +x scripts/script3_aux.sh
	scripts/script3.sh

.PHONY: clean cleanall all dummies
all: $(TARGETS)
clean cleanall:
	rm -rf $(BUILD_DIR)/* $(OBJ_DIR)/* logs/*.log *.sk test1 test2 test3 dummies* *.txt
	@touch $(BUILD_DIR)/.keep
	@touch $(OBJ_DIR)/.keep