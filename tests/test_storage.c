#include <stdio.h>
#include <assert.h>
#include <config.h>
#include <server_defines.h>
#include <storage.h>

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

	err = Storage_openFile(storage, "/home/liviusi/file7", 0, 1);
	assert(err == 0);
	err = Storage_openFile(storage, "/home/liviusi/file8", 0, 1);
	assert(err == 0);
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

	Storage_Print(storage);

	Storage_Free(storage);
	ServerConfig_Free(config);
}