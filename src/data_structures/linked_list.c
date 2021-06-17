/**
 * @brief Source file for linked_list header.
 * @author Giacomo Trapani.
*/

#include <linked_list.h>
#include <node.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifndef DEBUG
struct _linked_list
{
	node_t* first;
	node_t* last;
	unsigned long nelems;
	void (*free_data) (void*);
};
#endif

linked_list_t*
LinkedList_Init(void (*free_data) (void*))
{
	linked_list_t* tmp = (linked_list_t*) malloc(sizeof(linked_list_t));
	if (!tmp) goto no_more_memory;

	tmp->first = NULL;
	tmp->last = NULL;
	tmp->nelems = 0;
	if (!free_data) tmp->free_data = free;
	else tmp->free_data = free_data;

	return tmp;

	no_more_memory:
		errno = ENOMEM;
		return NULL;
}

const node_t*
LinkedList_GetFirst(const linked_list_t* list)
{
	return list->first;
}

const node_t*
LinkedList_GetLast(const linked_list_t* list)
{
	return list->last;
}

int
LinkedList_PushFront(linked_list_t* list, const char* key,
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

int
LinkedList_PopFront(linked_list_t* list, char** key, void** data)
{
	if (!list || !(list->nelems)) // list is either empty or null
	{
		errno = EINVAL;
		return -1;
	}
	list->nelems--;
	if ((key != NULL) && (Node_CopyKey(list->first, key) != 0))
	{
		if (errno == ENOMEM) return -1; // ENOMEM is considered a fatal error
		else *key = NULL; // EINVAL is not considered a fatal error
	}
	if ((data != NULL) && (Node_CopyData(list->first, data) == 0))
	{
		fprintf(stderr, "ERRNO = %d\n", errno);
		if (errno == ENOMEM) return -1; // ENOMEM is considered a fatal error
		else *data = NULL; // EINVAL is not considered a fatal error
	}
	if (list->nelems == 0)
	{
		Node_Free(list->first);
		list->first = NULL;
		list->last = NULL;
	}
	else
	{
		if (Node_ReplaceWithNext(&(list->first)) != 0) // errno has been set to EINVAL
			return -1;
	}
	// list->first has already been freed.
	return 0;
}

int
LinkedList_PopBack(linked_list_t* list, char** key, void** data)
{
	if (!list || !(list->nelems)) // list is either empty or null
	{
		errno = EINVAL;
		return -1;
	}
	list->nelems--;
	if ((key != NULL) && (Node_CopyKey(list->last, key) != 0))
	{
		if (errno == ENOMEM) return -1; // ENOMEM is considered a fatal error
		else *key = NULL; // EINVAL is not considered a fatal error
	}
	if ((data != NULL) && (Node_CopyData(list->last, data) == 0))
	{
		if (errno == ENOMEM) return -1; // ENOMEM is considered a fatal error
		else *data = NULL; // EINVAL is not considered a fatal error
	}
	if (list->nelems == 0)
	{
		Node_Free(list->first);
		list->first = NULL;
		list->last = NULL;
	}
	else
	{
		if (Node_ReplaceWithPrevious(&(list->last)) != 0) // errno has been set to EINVAL
			return -1;
	}
	return 0;
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
	const node_t* curr;
	char* tmp;
	int err;
	if (!list || !key) return 0;
	else
	{

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
}

void
LinkedList_Print(const linked_list_t* list)
{
	const node_t* curr = list->first;
	char* key = NULL;
	while (1)
	{
		if (curr == NULL)
		{
			fprintf(stdout, "NULL\n");
			break;
		}
		if (Node_CopyKey(curr, &key) != 0) fprintf(stdout, "NULL -> ");
		else fprintf(stdout, "%s -> ", key);
		curr = Node_GetNext(curr);
		free(key);
	}
	return;
}

void
LinkedList_Free(linked_list_t* list)
{
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