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
 * @brief Initializes empty storage data structure.
 * @returns Initialized data structure on success,
 * NULL on failure.
 * @param max_files_no cannot be 0.
 * @param max_storage_size cannot be 0.
 * @exception It sets "errno" to "EINVAL" if any param is not valid.
 * The function may also fail and set "errno" for any of the errors
 * specified for the routines "malloc", "RWLock_Init", "LinkedList_Init",
 * "HashTable_Init".
*/
storage_t*
Storage_Init(size_t max_files_no, size_t max_storage_size, replacement_algo_t chosen_algo);

/**
 * @brief Handles file opening.
 * @returns 0 on success, 1 on failure, 2 on fatal errors.
 * @param storage cannot be NULL.
 * @param pathname cannot be NULL.
 * @exception It sets "errno" to "EINVAL" if any param is not valid.
 * The function may also fail and set "errno" for any of the errors
 * specified for the routines "RWLock_ReadLock", "RWLock_ReadUnlock",
 * "RWLock_WriteLock", "RWLock_WriteUnlock", "LinkedList_PushFront",
 * "StoredFile_Init", "HashTable_Find", "HashTable_Insert",
 * "HashTable_GetPointerToData" which are all considered fatal errors;
 * non-fatal failures may happen because:
 *  	- storage is already full (sets "errno" to "ENOSPC");
 *  	- file has already been opened by this client (sets "errno" to "EBADF");
 *  	- another client currently own this file's lock and this client
 *  		is trying to acquire it (sets "errno" to "EACCES");
 *  	- client is trying to create an already existing file (sets "errno" to "EEXIST").
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
Storage_writeFile(storage_t* storage, const char* pathname,
			linked_list_t** evicted, int client);

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
Storage_closeFile(storage_t* storage, const char* pathname, int client);

/**
*/
int
Storage_removeFile(storage_t* storage, const char* pathname, int client);

/**
 * Utility print function.
*/
void
Storage_Print(const storage_t* storage);

/**
 * Frees allocated resources.
*/
void
Storage_Free(storage_t* storage);

#endif