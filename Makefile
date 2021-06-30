CC = gcc
CFLAGS += -std=c99 -Wall -g
INCLUDES = -I ./includes/
LIBS = -lpthread

BUILD_DIR := ./build
OBJ_DIR := ./obj
SRC_DIR := ./src
TEST_DIR := ./tests
HEADERS_DIR = ./includes/

TARGETS = test-config test-node test-linked-list test-hashtable test-storage server

OBJS-NODE = obj/node.o obj/test_node.o
OBJS-LINKEDLIST = obj/node.o obj/linked_list.o obj/test_linked_list.o
OBJS-HASHTABLE = obj/node.o obj/linked_list.o obj/hashtable.o obj/test_hashtable.o
OBJS-RWLOCK = obj/rwlock.o obj/test_rwlock.o
OBJS-CONFIG = obj/config.o obj/test_config.o
OBJS-STORAGE = obj/node.o obj/linked_list.o obj/hashtable.o obj/rwlock.o obj/config.o obj/storage.o obj/test_storage.o
OBJS-SERVER = obj/node.o obj/linked_list.o obj/hashtable.o obj/rwlock.o obj/config.o obj/storage.o obj/bounded_buffer.o obj/server.o
OBJS-CLIENT = obj/node.o obj/linked_list.o obj/server_interface.o obj/client.o

obj/node.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c src/data_structures/node.c $(LIBS)
	@mv node.o $(OBJ_DIR)/node.o

obj/test_node.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c tests/test_node.c $(LIBS)
	@mv test_node.o $(OBJ_DIR)/test_node.o

test-node: clean $(OBJS-NODE)
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/test-node $(OBJS-NODE)

obj/linked_list.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c src/data_structures/linked_list.c $(LIBS)
	@mv linked_list.o $(OBJ_DIR)/linked_list.o

test-linked-list: clean $(OBJS-LINKEDLIST)
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/test-linked_list $(OBJS-LINKEDLIST)

obj/test_linked_list.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c tests/test_linked_list.c $(LIBS)
	@mv test_linked_list.o $(OBJ_DIR)/test_linked_list.o

obj/hashtable.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c src/data_structures/hashtable.c $(LIBS)
	@mv hashtable.o $(OBJ_DIR)/hashtable.o

obj/test_hashtable.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c tests/test_hashtable.c $(LIBS)
	@mv test_hashtable.o $(OBJ_DIR)/test_hashtable.o

test-hashtable: clean $(OBJS-HASHTABLE)
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/test-hashtable $(OBJS-HASHTABLE)

obj/rwlock.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c src/data_structures/rwlock.c $(LIBS)
	@mv rwlock.o $(OBJ_DIR)/rwlock.o

obj/test_rwlock.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c tests/test_rwlock.c $(LIBS)
	@mv test_rwlock.o $(OBJ_DIR)/test_rwlock.o

test-rwlock: clean $(OBJS-RWLOCK)
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/test-rwlock $(OBJS-RWLOCK) $(LIBS)

obj/config.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c src/config.c $(LIBS)
	@mv config.o $(OBJ_DIR)/config.o

obj/test_config.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c tests/test_config.c $(LIBS)
	@mv test_config.o $(OBJ_DIR)/test_config.o

test-config: clean $(OBJS-CONFIG)
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/test-config $(OBJS-CONFIG)

obj/storage.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c src/storage.c $(LIBS)
	@mv storage.o $(OBJ_DIR)/storage.o

obj/test_storage.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c tests/test_storage.c $(LIBS)
	@mv test_storage.o $(OBJ_DIR)/test_storage.o

test-storage: clean $(OBJS-STORAGE)
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/test-storage $(OBJS-STORAGE) $(LIBS)

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

.PHONY: clean cleanall all
all: $(TARGETS)
clean cleanall:
	rm -r $(BUILD_DIR) $(OBJ_DIR)
	mkdir $(BUILD_DIR)
	mkdir $(OBJ_DIR)
	@touch $(BUILD_DIR)/.keep
	@touch $(OBJ_DIR)/.keep