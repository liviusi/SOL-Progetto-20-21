/**
 * @brief Source file for node header.
 * @author Giacomo Trapani.
*/

#include <node.h>
#include <stdio.h>
#include <string.h>

#ifndef DEBUG
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

node_t*
Node_Create(const char* key, size_t key_size, const void* data, size_t data_size, void (*free_data) (void*))
{
	node_t* tmp = NULL;
	char* tmp_key = NULL;
	void* tmp_data = NULL;
	tmp = (node_t*) malloc(sizeof(node_t));
	if (!tmp) goto no_more_memory;
	if (key_size != 0)
	{
		tmp_key = (char*) malloc(key_size);
		if (!tmp_key) goto no_more_memory;
		memcpy(tmp_key, key, key_size);
		
	}
	tmp->key = tmp_key;
	if (data_size != 0)
	{
		tmp_data = malloc(data_size);
		if (!tmp_data) goto no_more_memory;
		memcpy(tmp_data, data, data_size);
	}
	tmp->data = tmp_data;
	tmp->data_sz = data_size;
	tmp->next = NULL;
	tmp->prev = NULL;
	if (!free_data) tmp->free_data = free;
	else tmp->free_data = free_data;

	return tmp;

	no_more_memory:
		free(tmp_key);
		free(tmp_data);
		free(tmp);
		errno = ENOMEM;
		return NULL;
}

int
Node_SetNext(node_t* node1, const node_t* node2)
{
	if (!node1)
	{
		errno = EINVAL;
		return -1;
	}
	node1->next = (node_t*) node2;
	return 0;
}

int
Node_SetPrevious(node_t* node1, const node_t* node2)
{
	if (!node1)
	{
		errno = EINVAL;
		return -1;
	}
	node1->prev = (node_t*) node2;
	return 0;
}

const node_t*
Node_GetNext(const node_t* node)
{
	if (!node)
	{
		errno = EINVAL;
		return NULL;
	}
	return node->next;
}

const node_t*
Node_GetPrevious(const node_t* node)
{
	if (!node)
	{
		errno = EINVAL;
		return NULL;
	}
	return node->prev;
}

int
Node_CopyKey(const node_t* node, char** keyptr)
{
	if (!node || !(node->key))
	{
		errno = EINVAL;
		return -1;
	}
	size_t len = strlen(node->key);
	char* tmp;
	if ((tmp = (char*) malloc(sizeof(char) * (len+1))) == NULL)
	{
		errno = ENOMEM;
		return -1;
	}
	strncpy(tmp, node->key, len+1);
	*keyptr = tmp;
	return 0;
}

size_t
Node_CopyData(const node_t* node, void** dataptr)
{
	if (!node)
	{
		errno = EINVAL;
		return 0;
	}
	size_t len = node->data_sz;
	if (len == 0)
	{
		*dataptr = NULL;
		return len;
	}
	void* tmp;
	if ((tmp = malloc(len)) == NULL)
	{
		errno = ENOMEM;
		return 0;
	}
	memcpy(tmp, node->data, len);
	*dataptr = tmp;
	return len;
}

const void*
Node_GetData(const node_t* node)
{
	if (!node)
	{
		errno = EINVAL;
		return NULL;
	}
	return node->data;
}

int
Node_ReplaceWithNext(node_t** nodeptr)
{
	if (!nodeptr || !(*nodeptr))
	{
		errno = EINVAL;
		return -1;
	}
	node_t* node = *nodeptr;
	node_t* tmp = node->next;
	if (tmp && tmp->prev) tmp->prev = node->prev;
	if (node->prev) node->prev->next = tmp;
	Node_Free(node);
	*nodeptr = tmp;
	return 0;
}

int
Node_ReplaceWithPrevious(node_t** nodeptr)
{
	if (!nodeptr || !(*nodeptr))
	{
		errno = EINVAL;
		return -1;
	}
	node_t* node = *nodeptr;
	node_t* tmp = node->prev;
	if (tmp && tmp->next) tmp->next = node->next;
	if (node->next) node->next->prev = tmp;
	Node_Free(node);
	*nodeptr = tmp;
	return 0;
}

int
Node_Fold(node_t* node)
{
	if (!node)
	{
		errno = EINVAL;
		return -1;
	}
	if (node->prev) node->prev->next = node->next;
	if (node->next) node->next->prev = node->prev;
	Node_Free(node);
	return 0;
}

void
Node_Free(node_t* node)
{
	if (node)
	{
		if (node->prev) node->prev->next = node->next;
		if (node->next) node->next->prev = node->prev;
		free(node->key);
		node->free_data(node->data);
		free(node);
		node = NULL;
	}
	return;
}