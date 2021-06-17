/**
 * @brief Header file for node data structure.
 * @author Giacomo Trapani.
*/

#ifndef _NODE_H
#define _NODE_H

#include <stdlib.h>
#include <errno.h>

#ifdef DEBUG
struct _node
{
	char* key;
	unsigned long data_sz;
	void* data;
	struct _node* prev;
	struct _node* next;
	void (*free_data) (void*);
};
#endif

/**
 * Generic node struct used to save data inside doubly-linked-list data structure.
*/
typedef struct _node node_t;

/**
 * @brief Allocates memory for a new node and creates it with given key and data.
 * @returns Returns new node on success, NULL on failure.
 * @exception Sets errno to ENOMEM if and only if memory allocation fails.
*/
node_t*
Node_Create(const char* key, size_t key_size, const void* data,
		size_t data_size, void (*free_data) (void*));

/**
 * @brief Sets first param's next pointer to the second param.
 * @returns 0 on success, -1 on failure.
 * @exception It sets errno to EINVAL if and only if first param is NULL.
*/
int
Node_SetNext(node_t* node1, const node_t* node2);

/**
 * @brief Sets first param's previous pointer to the second param.
 * @returns 0 on success, -1 on failure.
 * @exception It sets errno to EINVAL if and only if first param is NULL.
*/
int
Node_SetPrevious(node_t* node1, const node_t* node2);

/**
 * @brief Getter for next node. Check whether errno has been set to EINVAL
 * when NULL is returned.
 * @returns Pointer to next node on success (which may be NULL), NULL on failure.
 * @exception It sets errno to EINVAL if and only if param is NULL.
*/
const node_t*
Node_GetNext(const node_t* node);

/**
 * @brief Getter for previous node. Check whether errno has been set to EINVAL
 * when NULL is returned.
 * @returns Pointer to previous node on success (which may be NULL), NULL on failure.
 * @exception It sets errno to EINVAL if and only if param is NULL.
*/
const node_t*
Node_GetPrevious(const node_t* node);

/**
 * @brief Copy node's key param to non-allocated char buffer.
 * @returns 0 on success, -1 on failure.
 * @exception It sets errno to EINVAL if and only if first param is NULL, (it sets errno to)
 * ENOMEM if and only if any memory allocation fails.
*/
int
Node_CopyKey(const node_t* node, char** keyptr);

/**
 * @brief Copy node's data param to non-allocated char buffer.
 * @returns Size of data buffer on success, 0 otherwise.
 * @exception It sets errno to EINVAL if and only if first param is NULL, (it sets errno to)
 * ENOMEM if and only if any memory allocation fails.
*/
size_t
Node_CopyData(const node_t* node, void** dataptr);

/**
 * @brief Gets a pointer to node's data param.
 * @returns Pointer to node's data on success (which may be NULL), NULL on failure.
 * @exception It sets errno to EINVAL if and only if param is NULL. 
*/
const void*
Node_GetData(const node_t* node);

/**
 * @brief Replaces node with the node its next field points to. It frees the given node struct.
 * @returns 0 on success, -1 on failure.
 * @exception It sets errno to EINVAL if and only if param is NULL.
*/
int
Node_ReplaceWithNext(node_t** nodeptr);

/**
 * @brief Replaces node with the node its previous field points to. It frees the given node struct.
 * @returns 0 on success, -1 on failure.
 * @exception It sets errno to EINVAL if and only if param is NULL.
*/
int
Node_ReplaceWithPrevious(node_t** nodeptr);

/**
 * @brief Makes param previous node point to param next node (i.e. param.prev.next = param.next, 
 * param.next.prev = param.prev). It frees the given node struct.
 * @returns 0 on success, -1 on failure.
 * @exception It sets errno to EINVAL if and only if param is NULL.
*/
int
Node_Fold(node_t* node);

/**
 * Frees allocated resources.
*/
void
Node_Free(node_t* node);

#endif