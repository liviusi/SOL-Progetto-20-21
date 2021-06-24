/**
 * @brief
 * @author Giacomo Trapani.
*/

#ifndef _BOUNDED_BUFFER_H_
#define _BOUNDED_BUFFER_H_

#include <stdlib.h>

typedef struct _bounded_buffer bounded_buffer_t;

bounded_buffer_t*
BoundedBuffer_Init(size_t capacity);

int
BoundedBuffer_Enqueue(bounded_buffer_t* buffer, const char* data);

int
BoundedBuffer_Dequeue(bounded_buffer_t* buffer, char** dataptr);

void
BoundedBuffer_Free(bounded_buffer_t*);

#endif