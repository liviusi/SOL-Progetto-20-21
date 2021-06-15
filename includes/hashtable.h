/**
 * @brief Header file for hash table data structure.
 * @author Giacomo Trapani.
*/

#ifndef _HASHTABLE_H_
#define _HASHTABLE_H_

#include <stdlib.h>
#include <linked_list.h>

/**
 * @brief Initializes empty hash table struct given number of buckets.
 * @returns Pointer to initialized table on success, NULL on failure.
 * @exception It sets errno to ENOMEM if and only if needed memory allocation fails.
*/
#define HASHTABLE_INITIALIZER(buckets_no) HashTable_Init(buckets_no, NULL, NULL);

// Struct fields are not exposed to maintain invariant.
typedef struct _hashtable hashtable_t;

/**
 * @brief Polynomial rolling hash function.
 * @param buffer buffer to hash
 * @return hash value of input buffer
*/
size_t
HashTable_HashFunction(const void*);

/**
 * String comparison.
*/
int
HashTable_Compare(const void*, const void*);

/**
 * @brief Initializes empty hash table struct given number of buckets, 
 * hash function and compare function.
 * @param buckets_no number of buckets
 * @param hash_function pointer to the function used for hashing; if NULL is passed then
 * it uses a default provided polynomial rolling hash function.
 * @param hash_compare pointer to the function used for key comparison; if NULL is passed then
 * it uses a default provided string comparison function.
 * @returns Pointer to initialized table on success, NULL on failure.
 * @exception It sets errno to ENOMEM if and only if needed memory allocation fails.
*/
hashtable_t*
HashTable_Init(size_t, size_t (const void*), int (const void*, const void*));

/**
 * @brief Creates and inserts entry into hashtable. Duplicates are not allowed.
 * @returns 1 on successful insertion, 0 if it is a duplicate, -1 otherwise. 
 * @exception It sets errno to ENOMEM if and only if needed memory allocation fails,
 * (it sets errno to) EINVAL if and only if key is NULL or key_size is zero.
*/
int
HashTable_Insert(hashtable_t*, const void*, size_t, const void*, size_t);

/**
 * @brief Checks whether hash table contains an entry for given key.
 * @returns 1 if such entry exists, 0 if it does not, -1 on failure.
 * @exception It sets errno to EINVAL if table or entry are NULL.
*/
int
HashTable_Find(const hashtable_t*, const void*);

/**
 * @brief Gets data corresponding to the given key, it will be written inside third param
 * (which must not refer to a pre-allocated buffer). Returns data size.
 * @returns Size of data buffer on success, 0 on failure.
 * @exception It sets errno to EINVAL if and only if table or entry are NULL,
 * (it sets errno to) ENOMEM if and only if needed memory allocation fails.
*/
size_t
HashTable_CopyOutData(const hashtable_t*, const void*, void**);

/**
 * @brief Gets pointer to data corresponding to the given key.
 * @returns Pointer to data.
 * @exception It sets errno to EINVAL if key or table are NULL.
*/
const void*
HashTable_GetPointerToData(const hashtable_t*, const void*);

/**
 * @brief Deletes node corresponding to given key from hash table.
 * @returns 1 on successful deletion, 0 if there was not such an entry, -1 on failure.
 * @exception It sets errno to EINVAL if and only if table or entry are NULL.
*/
int
HashTable_DeleteNode(hashtable_t*, const void*);

/**
 * Frees allocated resources.
*/
void
HashTable_Free(hashtable_t*);

/**
 * Utility print function.
*/
void
HashTable_Print(const hashtable_t*);

#endif