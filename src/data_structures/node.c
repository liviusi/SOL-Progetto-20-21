/**
 * @brief Source file for node header.
 * @author Giacomo Trapani.
*/

#include <node.h>
#include <stdio.h>
#include <string.h>
#include <wrappers.h>

struct _node
{
	char* key; // node's key
	unsigned long data_sz; // node's data size
	void* data; // generic data buffer
	struct _node* prev; // pointer to previous element
	struct _node* next; // pointer to next element
	void (*free_data) (void*); // pointer to function used to free data
};

node_t*
Node_Create(const char* key, size_t key_size, const void* data,
			size_t data_size, void (*free_data) (void*))
{
	if (!key || key_size == 0)
	{
		errno = EINVAL;
		return NULL;
	}
	int err;
	node_t* tmp = NULL;
	char* tmp_key = NULL;
	void* tmp_data = NULL;
	tmp = (node_t*) malloc(sizeof(node_t));
	GOTO_LABEL_IF_EQ(tmp, NULL, err, init_failure);
	if (key_size != 0)
	{
		tmp_key = (char*) malloc(key_size + 1);
		GOTO_LABEL_IF_EQ(tmp_key, NULL, err, init_failure);
		memset(tmp_key, 0, key_size + 1);
		memcpy(tmp_key, key, key_size);
	}
	tmp->key = tmp_key;
	if (data_size != 0)
	{
		tmp_data = malloc(data_size);
		GOTO_LABEL_IF_EQ(tmp_data, NULL, err, init_failure);
		memcpy(tmp_data, data, data_size);
	}
	tmp->data = tmp_data;
	tmp->data_sz = data_size;
	tmp->next = NULL;
	tmp->prev = NULL;
	tmp->free_data =  free_data ? free_data : free;

	return tmp;

	init_failure:
		err = errno;
		free(tmp_key);
		free(tmp_data);
		free(tmp);
		errno = err;
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
	if (!node || !(node->key) || !keyptr)
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
	if (!node || !dataptr)
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
	if ((tmp = malloc(len + 1)) == NULL)
	{
		errno = ENOMEM;
		return 0;
	}
	memset(tmp, 0, len+1);
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

void
Node_Remove(node_t* node)
{
	if (!node) return;
	if (node->prev) node->prev->next = node->next;
	if (node->next) node->next->prev = node->prev;
	Node_Free(node);
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
	}
	return;
}