/**
 * @brief
 * @author Giacomo Trapani.
*/

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include <bounded_buffer.h>
#include <linked_list.h>
#include <wrappers.h>

struct _bounded_buffer
{
	size_t capacity; // buffer's capacity
	linked_list_t* elems; // a linked list is used to avoid preallocating resources
	pthread_mutex_t mutex; // used to guarantee mutual exclusion over buffer
	pthread_cond_t full; // used to guarantee mutual exclusion over buffer
	pthread_cond_t empty; // used to guarantee mutual exclusion over buffer
};

bounded_buffer_t*
BoundedBuffer_Init(size_t capacity)
{
	if (capacity == 0)
	{
		errno = EINVAL;
		return NULL;
	}
	int err, errnocopy;
	linked_list_t* elems = NULL;
	pthread_mutex_t mutex;
	pthread_cond_t full;
	pthread_cond_t empty;
	bounded_buffer_t* tmp = NULL;
	bool full_initialized = false, empty_initialized = false;

	err = pthread_mutex_init(&mutex, NULL);
	err = pthread_cond_init(&full, NULL);
	GOTO_LABEL_IF_NEQ(err, 0, errnocopy, failure);
	full_initialized = true;
	err = pthread_cond_init(&empty, NULL);
	GOTO_LABEL_IF_NEQ(err, 0, errnocopy, failure);
	empty_initialized = true;
	elems = LinkedList_Init(NULL);
	GOTO_LABEL_IF_EQ(elems, NULL, errnocopy, failure);
	tmp = (bounded_buffer_t*) malloc(sizeof(bounded_buffer_t));
	GOTO_LABEL_IF_EQ(tmp, NULL, errnocopy, failure);

	tmp->capacity = capacity;
	tmp->elems = elems;
	tmp->mutex = mutex;
	tmp->full = full;
	tmp->empty = empty;

	return tmp;

	failure:
		pthread_mutex_destroy(&mutex);
		if (full_initialized) pthread_cond_destroy(&full);
		if (empty_initialized) pthread_cond_destroy(&full);
		LinkedList_Free(elems);
		errno = errnocopy;
		return NULL;
}

int
BoundedBuffer_Enqueue(bounded_buffer_t* buffer, const char* data)
{
	if (!buffer || !data)
	{
		errno = EINVAL;
		return -1;
	}
	int err;

	err = pthread_mutex_lock(&(buffer->mutex));
	if (err != 0) return -1;
	while (buffer->capacity == LinkedList_GetNumberOfElements(buffer->elems))
		pthread_cond_wait(&(buffer->full), &(buffer->mutex));
	err = LinkedList_PushBack(buffer->elems, data, strlen(data) + 1, NULL, 0);
	if (err != 0) return -1;
	if (LinkedList_GetNumberOfElements(buffer->elems) == 1)
		pthread_cond_broadcast(&(buffer->empty));
	err = pthread_mutex_unlock(&(buffer->mutex));
	if (err != 0) return -1;

	return 0;
}

int
BoundedBuffer_Dequeue(bounded_buffer_t* buffer, char** dataptr)
{
	if (!buffer)
	{
		errno = EINVAL;
		return -1;
	}
	int err;
	char* tmp = NULL;

	err = pthread_mutex_lock(&(buffer->mutex));
	if (err != 0) return -1;
	while (LinkedList_GetNumberOfElements(buffer->elems) == 0)
		pthread_cond_wait(&(buffer->empty), &(buffer->mutex));
	errno = 0;
	if (LinkedList_PopFront(buffer->elems, &tmp, NULL) == 0 && errno == ENOMEM) return -1;
	if (LinkedList_GetNumberOfElements(buffer->elems) == (buffer->capacity - 1))
		pthread_cond_broadcast(&(buffer->full));
	err = pthread_mutex_unlock(&(buffer->mutex));
	if (err != 0) return -1;

	if (dataptr) *dataptr = tmp;
	return 0;
}

void
BoundedBuffer_Free(bounded_buffer_t* buffer)
{
	if (!buffer) return;
	pthread_mutex_destroy(&(buffer->mutex));
	pthread_cond_destroy(&(buffer->empty));
	pthread_cond_destroy(&(buffer->full));
	LinkedList_Free(buffer->elems);
	free(buffer);
}