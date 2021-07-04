/**
 * @brief Header file for bounded buffer data structure and related operations.
 * @author Giacomo Trapani.
*/

#ifndef _BOUNDED_BUFFER_H_
#define _BOUNDED_BUFFER_H_

#include <stdlib.h>

// Struct fields are not exposed to force callee to access it using the implemented methods.
typedef struct _bounded_buffer bounded_buffer_t;

/**
 * @brief Initializes empty bounded buffer data structure given its capacity.
 * @returns Initialized data structure on success, NULL on failure.
 * @param capacity cannot be 0.
 * @exception It sets "errno" to "EINVAL" if any param is not valid.The function may also fail and set "errno"
 * for any of the errors specified for the routines "pthread_cond_init", "malloc", "LinkedList_Init".
*/
bounded_buffer_t*
BoundedBuffer_Init(size_t capacity);

/**
 * @brief Enqueues data to buffer.
 * @returns 0 on success, -1 on failure.
 * @param buffer cannot be NULL.
 * @param data cannot be NULL.
 * @exception It sets "errno" to "EINVAL" if any param is not valid. The function may also fail and set "errno"
 * for any of the errors specified for the routines "LinkedList_PushBack", "pthread_mutex_lock", "pthread_mutex_unlock".
*/
int
BoundedBuffer_Enqueue(bounded_buffer_t* buffer, const char* data);

/**
 * @brief Dequeues first element from buffer and copies it to non-allocated buffer.
 * @returns 0 on success, -1 on failure.
 * @param buffer cannot be NULL.
 * @param dataptr cannot be NULL.
 * @exception It sets "errno" to "EINVAL" if any param is not valid. The function may also fail and set "errno"
 * for any of the errors specified for the routines "LinkedList_PopFront", "pthread_mutex_lock", "pthread_mutex_unlock".
*/
int
BoundedBuffer_Dequeue(bounded_buffer_t* buffer, char** dataptr);

/**
 * Frees allocated resources.
*/
void
BoundedBuffer_Free(bounded_buffer_t*);

#endif