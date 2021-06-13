/**
 * @brief Header file for node data structure.
 * @author Giacomo Trapani.
*/

#ifndef _NODE_H
#define _NODE_H

#include <stdlib.h>
#include <errno.h>

/**
 * Generic node struct used to save data inside doubly-linked-list data structure.
 * It is composed of:
 * - 1x pointer to the next element;
 * - 1x pointer to the previous element;
 * - 1x C style string (hereinafter referred to as "key");
 * - 1x pointer to generic data ("data");
 * - 1x unsigned long denoting data's size.
*/
typedef struct _node node_t;

/**
 * @brief Allocates memory for a new node and creates it with given key and data.
 * @returns Returns new node on success, NULL on failure.
 * @exception Sets errno to ENOMEM if and only if memory allocation fails.
*/
node_t*
Node_Create(const char*, size_t, const void*, size_t);

/**
 * @brief Sets first param's next pointer to the second param.
 * @returns 0 on success, -1 on failure.
 * @exception It sets errno to EINVAL if and only if first param is NULL.
*/
int
Node_SetNext(node_t*, const node_t*);

/**
 * @brief Sets first param's previous pointer to the second param.
 * @returns 0 on success, -1 on failure.
 * @exception It sets errno to EINVAL if and only if first param is NULL.
*/
int
Node_SetPrevious(node_t*, const node_t*);

/**
 * @brief Getter for next node. Check whether errno has been set to EINVAL
 * when NULL is returned.
 * @returns Pointer to next node on success (which may be NULL), NULL on failure.
 * @exception It sets errno to EINVAL if and only if param is NULL.
*/
const node_t*
Node_GetNext(const node_t*);

/**
 * @brief Getter for previous node. Check whether errno has been set to EINVAL
 * when NULL is returned.
 * @returns Pointer to previous node on success (which may be NULL), NULL on failure.
 * @exception It sets errno to EINVAL if and only if param is NULL.
*/
const node_t*
Node_GetPrevious(const node_t*);

/**
 * @brief Copy node's key param to non-allocated char buffer.
 * @returns 0 on success, -1 on failure.
 * @exception It sets errno to EINVAL if and only if first param is NULL, (it sets errno to)
 * ENOMEM if and only if any memory allocation fails.
*/
int
Node_CopyKey(const node_t*, char**);

/**
 * @brief Copy node's data param to non-allocated char buffer.
 * @returns Size of data buffer on success, 0 otherwise.
 * @exception It sets errno to EINVAL if and only if first param is NULL, (it sets errno to)
 * ENOMEM if and only if any memory allocation fails.
*/
size_t
Node_CopyData(const node_t*, void**);

/**
 * @brief Replaces node with the node its next field points to. It frees the given node struct.
 * @returns 0 on success, -1 on failure.
 * @exception It sets errno to EINVAL if and only if param is NULL.
*/
int
Node_ReplaceWithNext(node_t**);

/**
 * @brief Replaces node with the node its previous field points to. It frees the given node struct.
 * @returns 0 on success, -1 on failure.
 * @exception It sets errno to EINVAL if and only if param is NULL.
*/
int
Node_ReplaceWithPrevious(node_t**);

/**
 * @brief Makes param previous node point to param next node (i.e. param.prev.next = param.next, 
 * param.next.prev = param.prev). It frees the given node struct.
 * @returns 0 on success, -1 on failure.
 * @exception It sets errno to EINVAL if and only if param is NULL.
*/
int
Node_Fold(node_t*);

/**
 * Frees allocated resources.
*/
void
Node_Free(node_t*);

#endif