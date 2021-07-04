/**
 * @brief Source file for linked_list header.
 * @author Giacomo Trapani.
*/

#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <node.h>
#include <wrappers.h>
#include <linked_list.h>


struct _linked_list
{
	node_t* first; // pointer to first element in list
	node_t* last; // pointer to last element in list
	unsigned long nelems; // number of elements in list
	void (*free_data) (void*); // pointer to function used to free nodes' data
};

linked_list_t*
LinkedList_Init(void (*free_data) (void*))
{
	linked_list_t* tmp = (linked_list_t*) malloc(sizeof(linked_list_t));
	if (!tmp) return NULL;

	tmp->first = NULL;
	tmp->last = NULL;
	tmp->nelems = 0;
	tmp->free_data = free_data ? free_data : free;

	return tmp;
}

const node_t*
LinkedList_GetFirst(const linked_list_t* list)
{
	if (!list) return NULL;
	return list->first;
}

const node_t*
LinkedList_GetLast(const linked_list_t* list)
{
	if (!list) return NULL;
	return list->last;
}

unsigned long
LinkedList_GetNumberOfElements(const linked_list_t* list)
{
	if (!list) return 0;
	return list->nelems;
}

int
LinkedList_PushFront(linked_list_t* list, const char* key,
		size_t key_size, const void* data, size_t data_size)
{
	if (!list || !key || key_size == 0)
	{
		errno = EINVAL;
		return -1;
	}

	node_t* tmp;
	if ((tmp = Node_Create(key, key_size, data, data_size, list->free_data)) == NULL)
		return -1;

	if (list->first == NULL) // list is empty if and only if the first elem is NULL
	{
		list->first = tmp;
		list->last = tmp;
	}
	else
	{
		Node_SetNext(tmp, list->first);
		Node_SetPrevious(list->first, tmp);
		list->first = tmp;
	}
	list->nelems++;

	return 0;
}

int
LinkedList_PushBack(linked_list_t* list, const char* key,
		size_t key_size, const void* data, size_t data_size)
{
	if (!list)
	{
		errno = EINVAL;
		return -1;
	}

	node_t* tmp;
	if ((tmp = Node_Create(key, key_size, data, data_size, list->free_data)) == NULL)
		return -1;
	
	if (list->first == NULL) // list is empty if and only if first elem is NULL
	{
		list->first = tmp;
		list->last = tmp;
	}
	else
	{
		Node_SetNext(list->last, tmp);
		Node_SetPrevious(tmp, list->last);
		list->last = tmp;
	}
	list->nelems++;

	return 0;
}

size_t
LinkedList_PopFront(linked_list_t* list, char** keyptr, void** dataptr)
{
	if (!list || !(list->nelems)) // list is either empty or null
	{
		errno = EINVAL;
		return 0;
	}
	list->nelems--;
	errno = 0;
	if ((keyptr != NULL) && (Node_CopyKey(list->first, keyptr) != 0))
	{
		if (errno == ENOMEM) return 0;
		else *keyptr = NULL;
	}
	size_t res = 0;
	errno = 0;
	if ((dataptr != NULL) && ((res = Node_CopyData(list->first, dataptr)) == 0))
	{
		if (errno == ENOMEM) return 0;
		else *dataptr = NULL;
	}
	if (list->nelems == 0)
	{
		Node_Free(list->first);
		list->first = NULL;
		list->last = NULL;
	}
	else
	{
		node_t* tmp = (node_t*) Node_GetNext(list->first);
		Node_SetPrevious(tmp, NULL);
		Node_Free(list->first);
		list->first = tmp;
	}
	// list->first has now been freed.
	return res;
}

size_t
LinkedList_PopBack(linked_list_t* list, char** keyptr, void** dataptr)
{
	if (!list || !(list->nelems)) // list is either empty or null
	{
		errno = EINVAL;
		return 0;
	}
	list->nelems--;
	errno = 0;
	if ((keyptr != NULL) && (Node_CopyKey(list->last, keyptr) != 0))
	{
		if (errno == ENOMEM) return 0;
		else *keyptr = NULL;
	}
	errno = 0;
	size_t res = 0;
	if ((dataptr != NULL) && ((res = Node_CopyData(list->last, dataptr)) == 0))
	{
		if (errno == ENOMEM) return 0;
		else *dataptr = NULL;
	}
	if (list->nelems == 0)
	{
		Node_Free(list->first);
		list->first = NULL;
		list->last = NULL;
	}
	else
	{
		node_t* tmp = (node_t*) Node_GetPrevious(list->last);
		Node_SetNext(tmp, NULL);
		Node_Free(list->last);
		list->last = tmp;
	}
	return res;
}

int
LinkedList_Remove(linked_list_t* list, const char* key)
{
	if (!list || !key)
	{
		errno = EINVAL;
		return -1;
	}
	node_t* curr = list->first;
	char* tmp;
	int err;
	while (curr != NULL)
	{
		err = Node_CopyKey(curr, &tmp);
		if (err == -1) return -1;
		if (strcmp(key, tmp) != 0)
		{
			free(tmp);
			curr = (node_t*) Node_GetNext(curr);
		}
		else
		{
			if (!Node_GetNext(curr)) list->last = (node_t*) Node_GetPrevious(curr);
			if (!Node_GetPrevious(curr)) list->first = (node_t*) Node_GetNext(curr);
			Node_Free(curr);
			free(tmp);
			list->nelems--;
			return 0;
		}
	}
	return 1;
}

int
LinkedList_IsEmpty(const linked_list_t* list)
{
	if (!list)
	{
		errno = EINVAL;
		return -1;
	}
	if (list->first == list->last && list->first == NULL) return 1;
	else return 0;
}

int
LinkedList_Contains(const linked_list_t* list, const char* key)
{
	if (!list || !key || !list->first) return 0;
	const node_t* curr;
	char* tmp;
	int err;
	for (curr = list->first; curr != NULL; curr = Node_GetNext(curr))
	{
		err = Node_CopyKey(curr, &tmp);
		if (err != 0) return -1;
		if (strcmp(tmp, key) != 0) free(tmp);
		else
		{
			free(tmp);
			return 1;
		}
	}
	return 0;
}

linked_list_t*
LinkedList_CopyAllKeys(const linked_list_t* list)
{
	if (!list || list->nelems == 0)
	{
		errno = EINVAL;
		return NULL;
	}
	linked_list_t* tmp = LinkedList_Init(NULL);
	if (!tmp) return NULL;
	char* key;
	const node_t* curr = list->first;
	int errnocopy;
	while (1)
	{
		if (curr == NULL) break;
		if (Node_CopyKey(curr, &key) != 0)
		{
			errnocopy = errno;
			LinkedList_Free(tmp);
			errno = errnocopy;
			return NULL;
		}
		if (LinkedList_PushBack(tmp, key, strlen(key) + 1, NULL, 0) != 0)
		{
			errnocopy = errno;
			LinkedList_Free(tmp);
			free(tmp);
			errno = errnocopy;
			return NULL;
		}
		free(key); key = NULL;
		curr = Node_GetNext(curr);
	}
	return tmp;
}

void
LinkedList_Print(const linked_list_t* list)
{
	if (!list) return;
	fprintf(stdout, "Current elements : %lu\n", list->nelems);
	const node_t* curr = list->first;
	char* key = NULL;
	while (1)
	{
		if (curr == NULL) break;
		if (Node_CopyKey(curr, &key) != 0) fprintf(stdout, "NULL -> ");
		else fprintf(stdout, "\t%s\n", key);
		curr = Node_GetNext(curr);
		free(key);
	}
	return;
}

void
LinkedList_Free(linked_list_t* list)
{
	if (!list) return;
	node_t* tmp;
	node_t* curr = list->first;
	while (1)
	{
		if (curr == NULL) break;
		tmp = curr;
		curr = (node_t*) Node_GetNext(curr);
		Node_Free(tmp);
	}
	free(list);
	return;
}