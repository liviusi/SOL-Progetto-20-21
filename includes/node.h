/**
 * @brief Header file for node data structure.
 * @author Giacomo Trapani.
*/

#ifndef _NODE_H
#define _NODE_H

#include <errno.h>
#include <stdlib.h>

// Generic node struct used to save data inside doubly-linked-list data structure.
typedef struct _node node_t;

/**
 * @brief Allocates memory for a new node and creates it with given key and data.
 * @returns Initialized node struct on success, NULL on failure.
 * @param key cannot be NULL.
 * @param key_size cannot be 0.
 * @param free_data pointer to function used to free node's data. It will be set to free if NULL.
 * @exception It sets "errno" to "EINVAL" if any param is not valid. The function may also fail and set "errno"
 * for any of the errors specified for the routine "malloc".
*/
node_t*
Node_Create(const char* key, size_t key_size, const void* data,
		size_t data_size, void (*free_data) (void*));

/**
 * @brief Sets node1's next to the second param.
 * @returns 0 on success, -1 on failure.
 * @param node1 cannot be NULL.
 * @exception It sets "errno" to "EINVAL" if any param is not valid.
*/
int
Node_SetNext(node_t* node1, const node_t* node2);

/**
 * @brief Sets node1's previous to the second param.
 * @returns 0 on success, -1 on failure.
 * @param node1 cannot be NULL.
 * @exception It sets "errno" to "EINVAL" if any param is not valid.
*/
int
Node_SetPrevious(node_t* node1, const node_t* node2);

/**
 * @brief Gets next node.
 * @returns Pointer to next node on success
 * (which may be NULL), NULL on failure.
 * @param node cannot be NULL.
 * @exception It sets "errno" to "EINVAL" if any param is not valid.
*/
const node_t*
Node_GetNext(const node_t* node);

/**
 * @brief Gets previous node.
 * @returns Pointer to previous node on success
 * (which may be NULL), NULL on failure.
 * @param node cannot be NULL.
 * @exception It sets "errno" to "EINVAL" if any param is not valid.
*/
const node_t*
Node_GetPrevious(const node_t* node);

/**
 * @brief Copies node's key to non-allocated char buffer.
 * @returns 0 on success, -1 on failure.
 * @param node cannot be NULL.
 * @param keyptr cannot be NULL.
 * @exception It sets "errno" to "EINVAL" if any param is not valid. The function may also fail and set "errno"
 * for any of the errors specified for the routine "malloc".
*/
int
Node_CopyKey(const node_t* node, char** keyptr);

/**
 * @brief Copies node's data param to non-allocated buffer.
 * @returns Size of data buffer on success, 0 otherwise.
 * @param node cannot be NULL.
 * @param dataptr cannot be NULL.
 * @exception It sets "errno" to "EINVAL" if any param is not valid. The function may also fail and set "errno"
 * for any of the errors specified for the routine "malloc".
*/
size_t
Node_CopyData(const node_t* node, void** dataptr);

/**
 * @brief Gets node's data param.
 * @returns Pointer to node's data on success
 * (which may be NULL), NULL on failure.
 * @param node cannot be NULL.
 * @exception It sets "errno" to "EINVAL" if any param is not valid.
*/
const void*
Node_GetData(const node_t* node);

/**
 * Removes node. It frees the given node.
*/
void
Node_Remove(node_t* node);

/**
 * Frees allocated resources.
*/
void
Node_Free(node_t* node);

#endif