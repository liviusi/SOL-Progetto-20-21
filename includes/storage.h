/**
 * @brief Header file for server storage.
 * @author Giacomo Trapani.
*/

#ifndef _STORAGE__H_
#define _STORAGE_H_

#include <stdlib.h>

#include <linked_list.h>
#include <server_defines.h>

// Struct fields are not exposed to force callee to access it using the implemented methods.
typedef struct _storage storage_t;

/**
 * @brief Initializes empty storage data structure.
 * @returns Initialized data structure on success,
 * NULL on failure.
 * @param max_files_no cannot be 0.
 * @param max_storage_size cannot be 0.
 * @exception It sets "errno" to "EINVAL" if any param is not valid.  The function may also fail and set "errno"
 * for any of the errors specified for the routines "malloc", "RWLock_Init", "LinkedList_Init", "HashTable_Init".
*/
storage_t*
Storage_Init(size_t max_files_no, size_t max_storage_size, replacement_policy_t chosen_algo);

/**
 * @brief Handles file opening.
 * @returns 0 on success, 1 on failure, 2 on fatal errors.
 * @param storage cannot be NULL.
 * @param pathname cannot be NULL.
 * @exception The function may fail and set "errno" for any of the errors specified for the routines "RWLock_ReadLock",
 * "RWLock_ReadUnlock", "RWLock_WriteLock", "RWLock_WriteUnlock", "LinkedList_PushFront", "LinkedList_Contains",
 * "StoredFile_Init", "HashTable_Find", "HashTable_Insert", "HashTable_GetPointerToData" which are all considered fatal errors.
 * Non-fatal failures may happen because:
 *  	- any param is not valid (sets "errno" to "EINVAL");
 *  	- storage is already full (sets "errno" to "ENOSPC");
 *  	- file has already been opened by this client (sets "errno" to "EBADF");
 *  	- another client owns this file's lock and this client is trying to acquire it (sets "errno" to "EACCES");
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
 * @exception The function may fail and set "errno" for any of the errors specified for the routines "RWLock_ReadLock",
 * "RWLock_ReadUnlock", "RWLock_WriteLock", "RWLock_WriteUnlock", "LinkedList_Contains", "HashTable_Find",
 * "HashTable_GetPointerToData", "malloc" which are all considered fatal errors.
 * Non-fatal failures may happen because:
 *  	- any param is not valid (sets "errno" to "EINVAL");
 *  	- another client owns this file's lock (sets "errno" to "EPERM");
 *  	- client has yet to open this file (sets "errno" to "EACCES");
 *  	- file is not inside the storage (sets "errno" to "EBADF").
*/
int
Storage_readFile(storage_t* storage, const char* pathname, void** buf, size_t* size, int client);

/**
 * @brief Handles reading up to n files from storage.
 * @returns 0 on success, 1 on failure, 2 on fatal errors.
 * @param storage cannot be NULL.
 * @param read_files cannot be NULL.
 * @param n if 0, every readable file is read.
 * @exception The function may fail and set "errno" for any of the errors specified for the routines "RWLock_ReadLock",
 * "RWLock_ReadUnlock", "LinkedList_CopyAllKeys", "LinkedList_Init", "LinkedList_PopFront", "HashTable_GetPointerToData",
 * "LinkedList_PushBack", "RWLock_WriteLock", "RWLock_WriteUnlock" which are all considered fatal errors.
 * Non-fatal failures may happen because:
 *  	- any param is not valid (sets "errno" to "EINVAL").
 * @note In this function: a file is considered readable if and only if it exists inside the storage and either its lock owner
 * has not been set or it is equal to given client.
*/
int
Storage_readNFiles(storage_t* storage, linked_list_t** read_files, size_t n, int client);

/**
 * @brief Handles file writing. May evict files from storage.
 * @returns 0 on success, 1 on failure, 2 on fatal errors.
 * @param storage cannot be NULL.
 * @param pathname cannot be NULL.
 * @exception The function may fail and set "errno" for any of the errors specified for the routines "malloc",
 * "RWLock_WriteLock", "RWLock_WriteUnlock", "HashTable_GetPointerToData", "HashTable_Find", "HashTable_DeleteNode",
 * "LinkedList_Init", "LinkedList_PushFront" which are all considered fatal errors.
 * Non-fatal failures may happen because:
 *  	- any param is not valid (sets "errno" to "EINVAL");
 *  	- file to be written is bigger than the whole storage (sets "errno" to "EFBIG");
 *  	- client is not a potential writer for the file (sets "errno" to "EACCES");
 *  	- file to be written has been evicted while attempting to free storage space (sets "errno" to "EIDRM");
 *  	- file is not inside the storage (sets "errno" to "EBADF").
*/
int
Storage_writeFile(storage_t* storage, const char* pathname, size_t length, const char* contents, linked_list_t** evicted, int client);

/**
 * @brief Handles append to file. May evict files from storage.
 * @returns 0 on success, 1 on failure, 2 on fatal errors.
 * @param storage cannot be NULL.
 * @param pathname cannot be NULL and must be a regular file.
 * @exception The function may fail and set "errno" for any of the errors specified for the routines "RWLock_WriteLock",
 * "RWLock_WriteUnlock", "HashTable_GetPointerToData", "HashTable_Find", "HashTable_DeleteNode", "LinkedList_Init",
 * "LinkedList_PushFront", "realloc" which are all considered fatal errors.
 * Non-fatal failures may happen because:
 *  	- any param is not valid (sets "errno" to "EINVAL");
 *  	- client has yet to open this file (sets "errno" to "EACCES");
 *  	- another client owns this file's lock (sets "errno" to "EPERM");
 *  	- file to be written has been evicted while attempting to free storage space (sets "errno" to "EIDRM");
 *  	- file is not inside the storage (sets "errno" to "EBADF").
*/
int
Storage_appendToFile(storage_t* storage, const char* pathname, void* buf, size_t size, linked_list_t** evicted, int client);

/**
 * @brief Handles file locking.
 * @returns 0 on success, 1 on failure, 2 on fatal errors.
 * @param storage cannot be NULL.
 * @param pathname cannot be NULL.
 * @exception The function may fail and set "errno" for any of the errors specified for the routines "RWLock_ReadLock",
 * "RWLock_ReadUnlock", "RWLock_WriteLock", "RWLock_WriteUnlock", "HashTable_Find", "HashTable_GetPointerToData",
 * "LinkedList_Contains", which are all considered fatal errors.
 * Non-fatal failures may happen because:
 *  	- any param is not valid (sets "errno" to "EINVAL");
 *  	- another client owns this file's lock (sets "errno" to "EPERM");
 *  	- client has yet to open this file (sets "errno" to "EACCES");
 *   	- file is not inside the storage (sets "errno" to "EBADF").
*/
int
Storage_lockFile(storage_t* storage, const char* pathname, int client);

/**
 * @brief Handles file unlocking.
 * @returns 0 on success, 1 on failure, 2 on fatal errors.
 * @exception The function may fail and set "errno" for any of the errors specified for the routines "RWLock_ReadLock",
 * "RWLock_ReadUnlock", "RWLock_WriteLock", "RWLock_WriteUnlock", "HashTable_Find", "HashTable_GetPointerToData",
 * "LinkedList_Contains", which are all considered fatal errors.
 * Non-fatal failures may happen because:
 *  	- any param is not valid (sets "errno" to "EINVAL");
 *  	- client has yet to open this file (sets "errno" to "EACCES");
 *  	- another client owns this file's lock (sets "errno" to "EPERM");
 *  	- file is not inside the storage (sets "errno" to "EBADF").
*/
int
Storage_unlockFile(storage_t* storage, const char* pathname, int client);

/**
 * @brief Handles file closure.
 * @returns 0 on success, 1 on failure, 2 on fatal errors.
 * @exception The function may fail and set "errno" for any of the errors  specified for the routines "RWLock_ReadLock",
 * "RWLock_ReadUnlock", "RWLock_WriteLock", "RWLock_WriteUnlock", "HashTable_Find", "HashTable_GetPointerToData",
 * "LinkedList_Contains", "LinkedList_Remove" which are all considered fatal errors.
 * Non-fatal failures may happen because:
 *  	- any param is not valid (sets "errno" to "EINVAL");
 *  	- client has yet to open this file (sets "errno" to "EACCES");
 *  	- file is not inside the storage (sets "errno" to "EBADF").
*/
int
Storage_closeFile(storage_t* storage, const char* pathname, int client);

/**
 * @brief Handles file removal.
 * @returns 0 on success, 1 on failure, 2 on fatal errors.
 * @exception The function may fail and set "errno" for any of the errors specified for the routines "RWLock_ReadLock",
 * "RWLock_ReadUnlock", "RWLock_WriteLock", "RWLock_WriteUnlock", "HashTable_Find", "HashTable_GetPointerToData",
 * "HashTable_DeleteNode", "LinkedList_Contains", "LinkedList_Remove" which are all considered fatal errors.
 * Non-fatal failures may happen because:
 *  	- any param is not valid (sets "errno" to "EINVAL");
 *  	- client has yet to open this file (sets "errno" to "EACCES");
 *  	- client has not locked this file (sets "errno" to "EPERM");
 *  	- file is not inside the storage (sets "errno" to "EBADF").
*/
int
Storage_removeFile(storage_t* storage, const char* pathname, int client);

/**
 * @brief Gets maximum amount of files stored.
 * @param storage cannot be NULL.
 * @returns Maximum amount of files stored on success (which may be 0), 0 on failure.
 * @exception It sets "errno" to "EINVAL" if any param is not valid. The function may also fail and set "errno"
 * for any of the errors specified for routines "RWLock_WriteLock", "RWLock_WriteUnlock".
*/
size_t
Storage_GetReachedFiles(storage_t* storage);

/**
 * @brief Gets maximum total size.
 * @param storage cannot be NULL.
 * @returns Maximum total size on success (which may be 0), 0 on failure.
 * @exception It sets "errno" to "EINVAL" if any param is not valid. The function may also fail and set "errno"
 * for any of the errors specified for routines "RWLock_WriteLock", "RWLock_WriteUnlock".
*/
size_t
Storage_GetReachedSize(storage_t* storage);

/**
 * Utility print function.
 * @note IT IS TO BE CALLED WHEN NO THREADS ARE WORKING ON GIVEN PARAM.
*/
void
Storage_Print(storage_t* storage);

/**
 * Frees allocated resources.
 * @note IT IS TO BE CALLED WHEN NO THREADS ARE WORKING ON GIVEN PARAM.
*/
void
Storage_Free(storage_t* storage);

#endif