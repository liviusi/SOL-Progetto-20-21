/**
 * @brief Header file for server storage.
 * @author Giacomo Trapani.
*/

#ifndef _STORAGE__H_
#define _STORAGE_H_

#include <stdlib.h>

typedef enum _replacement_algo
{
	FIFO
} replacement_algo_t;

#define DEBUG
#ifdef DEBUG
#include <rwlock.h>
#include <hashtable.h>
#include <linked_list.h>
#include <pthread.h>
struct _storage
{
	hashtable_t* files;
	replacement_algo_t algorithm; // right now only FIFO is to be supported.
	linked_list_t* sorted_files;

	size_t max_files_no;
	size_t max_storage_size;
	size_t files_no;
	size_t storage_size;

	rwlock_t* lock;
};
#endif

typedef struct _storage storage_t;

/**
*/
storage_t*
Storage_Init(size_t max_files_no, size_t max_storage_size, replacement_algo_t chosen_algo);

/**
*/
void
Storage_Print(const storage_t* storage);

/**
*/
void
Storage_Free(storage_t* storage);

/**
*/
int
Storage_openFile(storage_t* storage, const char* pathname, int flags, int client);

/**
*/
int
Storage_readFile(storage_t* storage, const char* pathname, void** buf,
			size_t* size, int client);

/**
*/
int
Storage_writeFile(storage_t* storage, const char* pathname, int client,
			linked_list_t** evicted);

/**
*/
int
Storage_appendToFile(storage_t* storage, const char* pathname, void* buf,
			size_t size, linked_list_t** evicted, int client);

/**
*/
int
Storage_lockFile(storage_t* storage, const char* pathname, int client);

/**
*/
int
Storage_unlockFile(storage_t* storage, const char* pathname, int client);

/**
*/
int
Storage_closeFile(storage_t*, const char*, int);

#endif