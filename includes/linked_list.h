/**
 * @brief Header file for linked list data structure.
 * @author Giacomo Trapani.
*/

#ifndef _LINKED_LIST_H_
#define _LINKED_LIST_H_
#include <stdlib.h>

// Struct elements are not exposed to maintain invariant.
typedef struct _linked_list linked_list_t;

/**
 * @brief Initializes empty linked list data structure.
 * @returns Initialized data structure on success, NULL on failure.
 * @exception It sets errno to ENOMEM if and only if needed memory allocation fails.
*/
linked_list_t* LinkedList_Init();

/**
 * @brief Creates and pushes node with given parameters to first position in linked list.
 * @returns 0 on success, -1 on failure.
 * @exception It sets errno to ENOMEM if and only if needed memory allocation fails,
 * (it sets errno to) EINVAL if and only if the first param is NULL.
*/
int
LinkedList_PushFront(linked_list_t*, const char*, size_t, const void*, size_t);

/**
 * @brief Creates and pushes node with given parameters to last position in linked list.
 * @returns 0 on success, -1 on failure.
 * @exception It sets errno to ENOMEM if and only if needed memory allocation fails,
 * (it sets errno to) EINVAL if and only if the first param is NULL.
*/
int
LinkedList_PushBack(linked_list_t*, const char*, size_t, const void*, size_t);

/**
 * @brief Pops first node from list and assigns its contents to second and third param.
 * NULL may be passed as second and third param: in this case no contents will be assigned.
 * @returns 0 on success, -1 on failure.
 * @exception It sets errno to EINVAL if list is empty or first param is NULL, (it sets errno to)
 * ENOMEM if and only if needed memory allocation fails.
*/
int
LinkedList_PopFront(linked_list_t*, char**, void**);

/**
 * @brief Pops last node from list and assigns its contents to second and third param.
 * NULL may be passed as second and third param: in this case no contents will be assigned.
 * @returns 0 on success, -1 on failure.
 * @exception It sets errno to EINVAL if list is empty or first param is NULL, (it sets errno to)
 * ENOMEM if and only if needed memory allocation fails.
*/
int
LinkedList_PopBack(linked_list_t*, char**, void**);

/**
 * Free allocated resources.
*/
void
LinkedList_Free(linked_list_t*);

/**
 * Utility print function.
*/
void
LinkedList_Print(linked_list_t* list);

#endif