/**
 * @brief Header file for linked list data structure.
 * @author Giacomo Trapani.
*/

#ifndef _LINKED_LIST_H_
#define _LINKED_LIST_H_

#include <stdlib.h>

#include <node.h>

// Struct fields are not exposed to maintain invariant.
typedef struct _linked_list linked_list_t;

/**
 * @brief Initializes empty linked list data structure.
 * @returns Initialized data structure on success, NULL on failure.
 * @param free_data pointer to function used to free node's data. It will be set to free if NULL.
 * @exception It sets "errno" for any of the errors specified for the routine "malloc".
*/
linked_list_t* LinkedList_Init(void (*free_data) (void*));

/**
 * @brief Gets first element of linked list.
 * @returns Pointer to first element of the list. NULL is returned if list is empty or NULL.
*/
const node_t*
LinkedList_GetFirst(const linked_list_t* list);

/**
 * @brief Gets last element of linked list.
 * @returns Pointer to last element of the list. NULL is returned if list is empty or NULL.
*/
const node_t*
LinkedList_GetLast(const linked_list_t* list);

/**
 * @brief Gets number of elements in linked list.
*/
unsigned long
LinkedList_GetNumberOfElements(const linked_list_t* list);

/**
 * @brief Creates and pushes node with given parameters to first position in linked list.
 * @returns 0 on success, -1 on failure.
 * @param key cannot be NULL.
 * @param key_size cannot be 0.
 * @exception It sets "errno" to "EINVAL" if any param is not valid. The function may also fail and set "errno"
 * for any of the errors specified for the routine "Node_Create".
*/
int
LinkedList_PushFront(linked_list_t* list, const char* key,
			size_t key_size, const void* data, size_t data_size);

/**
 * @brief Creates and pushes node with given parameters to last position in linked list.
 * @returns 0 on success, -1 on failure.
 * @param key cannot be NULL.
 * @param key_size cannot be 0.
 * @exception It sets "errno" to "EINVAL" if any param is not valid. The function may also fail and set "errno"
 * for any of the errors specified for the routine "Node_Create".
*/
int
LinkedList_PushBack(linked_list_t* list, const char* key,
			size_t key_size, const void* data, size_t data_size);

/**
 * @brief Pops first node from list and copies its fields to non-allocated buffers.
 * @returns Size of popped node's data on success (which may be 0), 0 on failure.
 * @param list cannot be NULL or empty.
 * @param keyptr if it is NULL, no content will be copied.
 * @param dataptr if it is NULL, no content will be copied.
 * @exception It sets "errno" to "EINVAL" if any param is not valid. The function may also fail and set "errno"
 * for any of the errors specified for the routines "Node_CopyKey", "Node_CopyData".
*/
size_t
LinkedList_PopFront(linked_list_t* list, char** keyptr, void** dataptr);

/**
 * @brief Pops last node from list and copies its fields to non-allocated buffers.
 * @returns Size of popped node's data on success (which may be 0), 0 on failure.
 * @param list cannot be NULL or empty.
 * @param keyptr if it is NULL, no content will be assigned.
 * @param dataptr if it is NULL, no content will be assigned.
 * @exception It sets "errno" to "EINVAL" if any param is not valid. The function may also fail and set "errno"
 * for any of the errors specified for the routines "Node_CopyKey", "Node_CopyData".
*/
size_t
LinkedList_PopBack(linked_list_t* list, char** keyptr, void** dataptr);

/**
 * @brief Removes first node with given key from list.
 * @returns 0 on successful deletion, 1 if such element does not exists, -1 on failure.
 * @param list cannot be NULL.
 * @param key cannot be NULL.
 * @exception It sets "errno" to "EINVAL" if any param is not valid. The function may also fail and set "errno"
 * for any of the errors specified for the routine "Node_CopyKey".
*/
int
LinkedList_Remove(linked_list_t* list, const char* key);

/**
 * @brief Checks whether list is empty.
 * @returns 1 if it is empty, 0 if it is not, -1 on failure.
 * @param list cannot be NULL.
 * @exception It sets "errno" to "EINVAL" if any param is not valid.
*/
int
LinkedList_IsEmpty(const linked_list_t* list);

/**
 * @brief Checks whether list contains given key.
 * @returns 1 if it contains the key, 0 if such element does not exist, -1 on failure.
 * @exception It sets "errno" for any of the errors specified for the routine "Node_CopyKey".
*/
int
LinkedList_Contains(const linked_list_t* list, const char* key);

/**
 * @brief Copies all given list's keys into a new list.
 * @returns Copied list on success, NULL on failure.
 * @param list cannot be NULL.
 * @exception It sets "errno" to "EINVAL" if any param is not valid. The function may also fail and set "errno"
 * for any of the errors specified for the routines "LinkedList_Init", "Node_CopyKey", "LinkedList_PushBack".
*/
linked_list_t*
LinkedList_CopyAllKeys(const linked_list_t* list);

/**
 * Utility print function.
*/
void
LinkedList_Print(const linked_list_t* list);

/**
 * Frees allocated resources.
*/
void
LinkedList_Free(linked_list_t*);

#endif