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
Storage_Init(size_t, size_t, replacement_algo_t);

/**
*/
void
Storage_Print(const storage_t*);

/**
*/
void
Storage_Free(storage_t*);

/**
*/
int
Storage_openFile(storage_t*, const char*, int, int);

/**
*/
int
Storage_readFile(storage_t*, const char*, void**, size_t*, int);

/**
*/
int
Storage_writeFile(storage_t* storage, const char* pathname, int client, linked_list_t** expelled);

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