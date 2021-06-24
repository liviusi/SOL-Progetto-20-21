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
 * @exception The function may fail and set "errno" for any of the errors
 * specified for the routines "RWLock_ReadLock", "RWLock_ReadUnlock",
 * "RWLock_WriteLock", "RWLock_WriteUnlock", "LinkedList_PushFront",
 * "LinkedList_Contains", "StoredFile_Init", "HashTable_Find", "HashTable_Insert",
 * "HashTable_GetPointerToData" which are all considered fatal errors.
 * Non-fatal failures may happen because:
 *  	- any param is not valid (sets "errno" to "EINVAL");
 *  	- storage is already full (sets "errno" to "ENOSPC");
 *  	- file has already been opened by this client (sets "errno" to "EBADF");
 *  	- another client owns this file's lock and this client
 *  		is trying to acquire it (sets "errno" to "EACCES");
 *  	- client is trying to create an already existing file (sets "errno" to "EEXIST").
*/
int
Storage_openFile(storage_t* storage, const char* pathname, int flags, int client);

/**
 * @brief Handles file reading.
 * @returns 0 on success, 1 on failure, 2 on fatal errors.
 * @param storage cannot be NULL.
 * @param pathname cannot be NULL.
 * @param buf cannot be NULL.
 * @param size cannot be NULL.
 * @exception The function may fail and set "errno" for any of the errors
 * specified for the routines "RWLock_ReadLock", "RWLock_ReadUnlock",
 * "RWLock_WriteLock", "RWLock_WriteUnlock", "LinkedList_Contains",
 * "HashTable_Find", "HashTable_GetPointerToData", "malloc" which
 * are all considered fatal errors.
 * Non-fatal failures may happen because:
 *   	- any param is not valid (sets "errno" to "EINVAL");
 *   	- another client owns this file's lock (sets "errno" to "EACCES");
 *  	- client has yet to open this file (sets "errno" to "EACCES");
 *  	- file is not inside the storage (sets "errno" to "EBADF").
*/
int
Storage_readFile(storage_t* storage, const char* pathname, void** buf,
			size_t* size, int client);

/**
 * @brief Handles file writing. May evict files from storage.
 * @returns 0 on success, 1 on failure, 2 on fatal errors.
 * @param storage cannot be NULL.
 * @param pathname cannot be NULL and must be a regular file.
 * @exception The function may fail and set "errno" for any of the errors
 * specified for the routines "malloc", "RWLock_WriteLock", "RWLock_WriteUnlock",
 * "HashTable_GetPointerToData", "HashTable_Find", "HashTable_DeleteNode",
 * "LinkedList_Init", "LinkedList_PushFront" which are all considered fatal errors.
 * Non-fatal failures may happen because:
 *   	- any param is not valid (sets "errno" to "EINVAL");
 *   	- routines "fopen", "ftell", "fseek", "fread", "is_regular_file" fail
 *   		(thus properly setting "errno");
 *   	- file to be written is bigger than the whole storage;
 *   	- client is not a potential writer for the file (sets "errno" to "EACCES");
 *   	- file to be written has been evicted while attempting to free
 *   		storage space (sets "errno" to "EIDRM");
 *   	- file is not inside the storage (sets "errno" to "EBADF").
*/
int
Storage_writeFile(storage_t* storage, const char* pathname,
			linked_list_t** evicted, int client);

/**
 * @brief Handles append to file. May evict files from storage.
 * @returns 0 on success, 1 on failure, 2 on fatal errors.
 * @param storage cannot be NULL.
 * @param pathname cannot be NULL and must be a regular file.
 * @exception The function may fail and set "errno" for any of the errors
 * specified for the routines "RWLock_WriteLock", "RWLock_WriteUnlock",
 * "HashTable_GetPointerToData", "HashTable_Find", "HashTable_DeleteNode",
 * "LinkedList_Init", "LinkedList_PushFront", "realloc" which are all
 * considered fatal errors.
 * Non-fatal failures may happen because:
 *  	- any param is not valid (sets "errno" to "EINVAL");
 *  	- client has yet to open this file (sets "errno" to "EACCES");
 *  	- another client owns this file's lock (sets "errno" to "EACCES");
 *  	- file to be written has been evicted while attempting to
 *  		free storage space (sets "errno" to "EIDRM");
 *  	- file is not inside the storage (sets "errno" to "EBADF").
*/
int
Storage_appendToFile(storage_t* storage, const char* pathname, void* buf,
			size_t size, linked_list_t** evicted, int client);

/**
 * @brief Handles file locking.
 * @returns 0 on success, 1 on failure, 2 on fatal errors.
 * @param storage cannot be NULL.
 * @param pathname cannot be NULL.
 * @exception The function may fail and set "errno" for any of the errors
 * specified for the routines "RWLock_ReadLock", "RWLock_ReadUnlock",
 * "RWLock_WriteLock", "RWLock_WriteUnlock", "HashTable_Find",
 * "HashTable_GetPointerToData", "LinkedList_Contains",
 * "pthread_mutex_lock", "pthread_mutex_unlock" which are
 * all considered fatal errors.
 * Non-fatal failures may happen because:
 *  	- any param is not valid (sets "errno" to "EINVAL");
 *  	- client has yet to open this file (sets "errno" to "EACCES");
 *   	- file is not inside the storage (sets "errno" to "EBADF").
*/
int
Storage_lockFile(storage_t* storage, const char* pathname, int client);

/**
 * @brief Handles file unlocking.
 * @returns 0 on success, 1 on failure, 2 on fatal errors.
 * @exception The function may fail and set "errno" for any of the errors
 * specified for the routines "RWLock_ReadLock", "RWLock_ReadUnlock",
 * "RWLock_WriteLock", "RWLock_WriteUnlock", "HashTable_Find",
 * "HashTable_GetPointerToData", "LinkedList_Contains",
 * "pthread_mutex_lock", "pthread_mutex_unlock" which are
 * all considered fatal errors.
 * Non-fatal failures may happen because:
 *  	- any param is not valid (sets "errno" to "EINVAL");
 *  	- client has yet to open this file (sets "errno" to "EACCES");
 *  	- another client owns this file's lock (sets "errno" to "EACCES");
 *   	- file is not inside the storage (sets "errno" to "EBADF").
*/
int
Storage_unlockFile(storage_t* storage, const char* pathname, int client);

/**
 * @brief Handles file closure.
 * @returns 0 on success, 1 on failure, 2 on fatal errors.
 * @exception The function may fail and set "errno" for any of the errors
 * specified for the routines "RWLock_ReadLock", "RWLock_ReadUnlock",
 * "RWLock_WriteLock", "RWLock_WriteUnlock", "HashTable_Find",
 * "HashTable_GetPointerToData", "LinkedList_Contains",
 * "LinkedList_Remove" which are all considered fatal errors.
 * Non-fatal failures may happen because:
 *  	- any param is not valid (sets "errno" to "EINVAL");
 *  	- client has yet to open this file (sets "errno" to "EACCES");
 *   	- file is not inside the storage (sets "errno" to "EBADF").
*/
int
Storage_closeFile(storage_t* storage, const char* pathname, int client);

/**
 * @brief Handles file removal.
 * @returns 0 on success, 1 on failure, 2 on fatal errors.
 * @exception The function may fail and set "errno" for any of the errors
 * specified for the routines "RWLock_ReadLock", "RWLock_ReadUnlock",
 * "RWLock_WriteLock", "RWLock_WriteUnlock", "HashTable_Find",
 * "HashTable_GetPointerToData", "HashTable_DeleteNode",
 * "LinkedList_Contains", "LinkedList_Remove".
 * Non-fatal failures may happen because:
 *  	- any param is not valid (sets "errno" to "EINVAL");
 *  	- client has yet to open this file (sets "errno" to "EACCES");
 *   	- client has not locked this file (sets "errno" to "EACCES");
 *   	- file is not inside the storage (sets "errno" to "EBADF").
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