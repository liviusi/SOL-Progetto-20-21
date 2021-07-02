#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <config.h>
#include <server_defines.h>
#include <storage.h>

#define STRING_SAMPLE "\nLorem ipsum dolor sit amet!!\n"

int main(int argc, char* argv[])
{
	server_config_t* config = ServerConfig_Init();
	ServerConfig_Set(config, "/home/liviusi/Desktop/SOL-Progetto-20-21/config.txt");
	size_t files, size;
	files = (size_t) ServerConfig_GetMaxFilesNo(config);
	size = (size_t) ServerConfig_GetStorageSize(config);
	storage_t* storage = Storage_Init(files, size, FIFO);
	int err;
	err = Storage_openFile(storage, "/home/liviusi/file1", O_CREATE, 1);
	assert(err == 0);
	err = Storage_openFile(storage, "/home/liviusi/file2", O_CREATE, 1);
	assert(err == 0);
	err = Storage_openFile(storage, "/home/liviusi/file3", O_CREATE, 1);
	assert(err == 0);
	err = Storage_openFile(storage, "/home/liviusi/file4", O_CREATE, 1);
	assert(err == 0);
	err = Storage_openFile(storage, "/home/liviusi/file5", 0 | O_CREATE | O_LOCK, 1);
	assert(err == 0);
	err = Storage_openFile(storage, "/home/liviusi/file1", 0, 2);
	assert(err == 0);
	err = Storage_openFile(storage, "/home/liviusi/file1", 0, 1);
	assert(err == 1);

	err = Storage_openFile(storage, "/home/liviusi/file7", O_CREATE, 1);
	assert(err == 0);
	err = Storage_openFile(storage, "/home/liviusi/file7", 0, 2);
	assert(err == 0);
	err = Storage_openFile(storage, "/home/liviusi/file8", 0, 1);
	assert(err == 1);
	//Storage_Print(storage);

	err = Storage_closeFile(storage, "/home/liviusi/file1", 0);
	assert(err == 1);
	err = Storage_closeFile(storage, "/home/liviusi/file1", 1);
	assert(err == 0);
	err = Storage_closeFile(storage, "/home/liviusi/file1", 2);
	assert(err == 0);
	err = Storage_closeFile(storage, "/home/liviusi/file1", 2);
	assert(err == 1);

	char* buffer = NULL; size_t buffer_size = 1;
	err = Storage_readFile(storage, "/home/liviusi/file5", (void**) &buffer, &buffer_size, 0);
	assert(err == 1);
	free(buffer); buffer = NULL;
	//printf("size = %lu, buffer = %s\n", buffer_size, buffer); buffer_size = 1;
	err = Storage_readFile(storage, "/home/liviusi/file5", (void**) &buffer, &buffer_size, 1);
	assert(err == 0);
	free(buffer); buffer = NULL;
	//printf("size = %lu, buffer = %s\n", buffer_size, buffer); buffer_size = 1;
	err = Storage_readFile(storage, "/home/liviusi/file6", (void**) &buffer, &buffer_size, 0);
	assert(err == 1);
	free(buffer); buffer = NULL;
	//printf("size = %lu, buffer = %s\n", buffer_size, buffer); buffer_size = 1;

	//Storage_Print(storage);

	err = Storage_lockFile(storage, "/home/liviusi/file5", 1);
	assert(err == 0);
	err = Storage_unlockFile(storage, "/home/liviusi/file5", 1);
	assert(err == 0);
	err = Storage_lockFile(storage, "/home/liviusi/file5", 1);
	assert(err == 0);
	err = Storage_lockFile(storage, "/home/liviusi/file4", 1);
	assert(err == 0);
	err = Storage_lockFile(storage, "/home/liviusi/file1", 42);
	assert(err == 1);
	err = Storage_unlockFile(storage, "/home/liviusi/file1", 42);
	assert(err == 1);
	err = Storage_lockFile(storage, "/home/liviusi/file7", 1);
	assert(err == 0);
	
	//err = Storage_lockFile(storage, "/home/liviusi/file7", 2);
	//assert(err == 0);
	//err = Storage_unlockFile(storage, "/home/liviusi/file7", 1);
	//assert(err == 0);
	//Storage_Print(storage);


	err = Storage_openFile(storage, "/home/liviusi/Desktop/SOL-Progetto-20-21/Makefile", O_CREATE|O_LOCK, 5);
	assert(err == 0);
	//Storage_Print(storage);
	err = Storage_removeFile(storage, "/home/liviusi/file4", 1);
	assert(err == 0);
	err = Storage_openFile(storage, "/home/liviusi/Desktop/SOL-Progetto-20-21/src/data_structures/rwlock.c", O_CREATE|O_LOCK, 3);
	assert(err == 0);
	//Storage_Print(storage);

	err = Storage_openFile(storage, "/home/liviusi/Desktop/SOL-Progetto-20-21/includes/wrappers.h", O_CREATE|O_LOCK, 5);
	assert(err == 0);
	//Storage_Print(storage);
	//LinkedList_Print(list);
	linked_list_t* list = NULL;
	char* string = (char*) malloc(sizeof(char) * 100);
	strcpy(string, STRING_SAMPLE);
	string[strlen(STRING_SAMPLE) + 1] = '\0';
	err = Storage_appendToFile(storage, "/home/liviusi/Desktop/SOL-Progetto-20-21/src/data_structures/rwlock.c", (void*) string, strlen(string) + 1, &list, 3);
	//fprintf(stderr, "errno = %d\n", errno);
	assert(err == 0);
	free(string);
	//Storage_Print(storage);
	//LinkedList_Print(list);
	LinkedList_Free(list);

	linked_list_t* read = NULL;
	err = Storage_readNFiles(storage, &read, 0, 1);
	assert (err == 0);
	LinkedList_Print(read); LinkedList_Free(read);
	Storage_Print(storage);

	Storage_Free(storage);
	ServerConfig_Free(config);
}