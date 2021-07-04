/**
 * @brief Header file for hash table data structure.
 * @author Giacomo Trapani.
*/

#ifndef _HASHTABLE_H_
#define _HASHTABLE_H_

#include <stdlib.h>

#include <linked_list.h>

// Struct fields are not exposed to maintain invariant.
typedef struct _hashtable hashtable_t;

/**
 * @brief Initializes empty hash table struct.
 * @returns Pointer to initialized table on success, NULL on failure.
 * @param buckets_no number of buckets
 * @param hash_function pointer to function used for hashing. It will be set to a default provided polynomial
 * rolling hash function if NULL.
 * @param hash_compare pointer to function used for key comparison. It will be set to a default provided string
 * comparison function if NULL.
 * @param free_data pointer to function used to free node's data. It will be set to free if NULL.
 * @exception It sets "errno" for any of the errors specified for the routines "malloc", "LinkedList_Init".
*/
hashtable_t*
HashTable_Init(size_t buckets_no, size_t (*hash_function) (const void*), 
		int (*hash_compare) (const void*, const void*), void (*free_data) (void*));

/**
 * @brief Creates and inserts entry into table. Duplicates are not allowed.
 * @returns 1 on successful insertion, 0 if it is a duplicate, -1 otherwise. 
 * @param key cannot be NULL.
 * @param key_size cannot be 0.
 * @exception It sets "errno" to EINVAL if any param is not valid. The function may also fail and set "errno"
 * for any of the errors specified for the routines "Node_CopyKey", "LinkedList_PushBack".
*/
int
HashTable_Insert(hashtable_t* table, const void* key,
		size_t key_size, const void* data, size_t data_size);

/**
 * @brief Checks whether table contains an entry for given key.
 * @returns 1 if such entry exists, 0 if it does not, -1 on failure.
 * @param table cannot be NULL.
 * @param key cannot be NULL.
 * @exception It sets "errno" to "EINVAL" if any param is not valid. The function may also fail and set "errno"
 * for any of the errors specified for the routine "Node_CopyKey".
*/
int
HashTable_Find(const hashtable_t* table, const void* key);

/**
 * @brief Gets data corresponding to the given key to non-allocated buffer.
 * @returns Size of data buffer on success, 0 on failure.
 * @param table cannot be NULL.
 * @param key cannot be NULL.
 * @param dataptr cannot be NULL.
 * @exception It sets "errno" to "EINVAL" if any param is not valid. The function may also fail and set "errno"
 * for any of the errors specified for the routines "Node_CopyKey", "Node_CopyData", "Node_GetNext".
*/
size_t
HashTable_CopyOutData(const hashtable_t* table, const void* key, void** dataptr);

/**
 * @brief Gets pointer to data corresponding to the given key.
 * @returns Pointer to data on success (which may be NULL), NULL on failure.
 * @param table cannot be NULL.
 * @param key cannot be NULL.
 * @exception It sets "errno" to "EINVAL" if any param is not valid; if the function fails because given key does
 * not belong to the set of those inside the table, "errno" is set to "ENOENT". The function may also fail and set "errno"
 * for any of the errors specified for the routine "Node_CopyKey".
*/
const void*
HashTable_GetPointerToData(const hashtable_t* table, const void* key);

/**
 * @brief Deletes node corresponding to given key from table.
 * @returns 1 on successful deletion, 0 if such entry
 * does not exist, -1 on failure.
 * @param table cannot be NULL.
 * @param key cannot be NULL.
 * @exception It sets "errno" to "EINVAL" if any param is not valid. The function may also fail and set "errno"
 * for any of the errors specified for the routine "LinkedList_Remove".
*/
int
HashTable_DeleteNode(hashtable_t* table, const void* key);

/**
 * Frees allocated resources.
*/
void
HashTable_Free(hashtable_t* table);

/**
 * Utility print function.
*/
void
HashTable_Print(const hashtable_t* table);

#endif