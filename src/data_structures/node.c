/**
 * @brief Source file for node header.
 * @author Giacomo Trapani.
*/

#include <node.h>
#include <string.h>

struct _node
{
	char* key;
	unsigned long data_sz;
	void* data;
	struct _node* prev;
	struct _node* next;
};

node_t*
Node_Create(const char* key, size_t key_size, const void* data, size_t data_size)
{
	node_t* tmp = (node_t*) malloc(sizeof(node_t));
	if (!tmp) goto no_more_memory;
	if (key_size != 0)
	{
		tmp->key = (char*) malloc(key_size);
		if (!(tmp->key))
		{
			free (tmp);
			goto no_more_memory;
		}
		memcpy(tmp->key, key, key_size);
	}
	else tmp->key = NULL;
	if (data_size != 0)
	{
		tmp->data = malloc(data_size);
		if (!(tmp->data))
		{
			free(tmp->key);
			free(tmp);
			goto no_more_memory;
		}
		memcpy(tmp->data, data, data_size);
	}
	else tmp->data = NULL;
	tmp->data_sz = data_size;
	tmp->next = NULL;
	tmp->prev = NULL;

	return tmp;

	no_more_memory:
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

int
Node_CopyData(const node_t* node, void** dataptr)
{
	if (!node || !(node->data))
	{
		errno = EINVAL;
		return -1;
	}
	size_t len = node->data_sz;
	void* tmp;
	if ((tmp = malloc(len)) == NULL)
	{
		errno = ENOMEM;
		return -1;
	}
	memcpy(tmp, node->data, len);
	*dataptr = tmp;
	return 0;
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
	Node_FreeStruct(node);
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
	Node_FreeStruct(node);
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
	node->prev->next = node->next;
	node->next->prev = node->prev;
	Node_FreeStruct(node);
	return 0;
}

void
Node_FreeKey(node_t* node)
{
	if (node) free(node->key);
	return;
}

void
Node_FreeData(node_t* node)
{
	if (node) free(node->data);
	return;
}

void
Node_FreeStruct(node_t* node)
{
	if (node)
	{
		Node_FreeKey(node);
		Node_FreeData(node);
		free(node);
	}
	return;
}