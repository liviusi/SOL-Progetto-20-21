/**
 * @brief Source file for hashtable header.
 * @author Giacomo Trapani.
*/

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <node.h>
#include <hashtable.h>
#include <linked_list.h>

struct _hashtable
{
	size_t buckets_no; // number of buckets
	linked_list_t** buckets; // array of pointer to linked lists denoting buckets
	size_t (*hash_function) (const void*); // pointer to hash function
	int (*hash_compare) (const void*, const void*); // pointer to key comparison function
	void (*free_data) (void*); // pointer to data freeing function
};

/**
 * @brief Polynomial rolling hash function.
 * @param buffer buffer to hash
 * @return hash value of input buffer
*/
static size_t
HashTable_HashFunction(const void* buffer)
{
	if (buffer == NULL) return 0;
	char* tmp = (char*) buffer;
	// roughly equal to the number of letters
	// in the alphabet (uppercase and lowercase)
	const int p = 53;
	const int m = 1e9 + 9; // large prime number
	size_t hash_value = 0;
	long long p_pow = 1;
	for (size_t i = 0; tmp[i] != '\0'; i++)
	{
		char c = tmp[i];
		hash_value = (hash_value + (c - 'a' + 1) * p_pow) % m;
		p_pow = (p_pow * p) % m;
	}
	return hash_value;
}

/**
 * String comparison.
*/
static int
HashTable_Compare(const void* a, const void* b)
{
	return strcmp((char*) a, (char*) b);
}

hashtable_t*
HashTable_Init(size_t buckets_no, size_t (*hash_function) (const void*), 
		int (*hash_compare) (const void*, const void*), void (*free_data) (void*))
{
	hashtable_t* table = (hashtable_t*) malloc(sizeof(hashtable_t));
	if (table == NULL)
	{
		errno = ENOMEM;
		return NULL;
	}
	table->buckets_no = buckets_no;
	table->buckets = (linked_list_t**) malloc(sizeof(linked_list_t*) * buckets_no);
	if (!(table->buckets))
	{
		errno = ENOMEM;
		free(table);
		return NULL;
	}
	size_t i = 0;
	for (; i < buckets_no; i++)
	{
		if (!free_data)
			(table->buckets)[i] = LinkedList_Init(free);
		else
			(table->buckets)[i] = LinkedList_Init(free_data);
		if (!((table->buckets)[i]))
		{
			size_t j = 0;
			while (j != i)
			{
				LinkedList_Free((table->buckets)[j]);
				j++;
			}
			free(table->buckets);
			free(table);
			errno = ENOMEM;
			return NULL;
		}
	}
	table->hash_function = ((!hash_function) ? (HashTable_HashFunction) : (hash_function));
	table->hash_compare = ((!hash_compare) ? (HashTable_Compare) : (hash_compare));
	table->free_data = ((!free_data) ? (free) : (free_data));
	return table;
}

int
HashTable_Insert(hashtable_t* table, const void* key,
		size_t key_size, const void* data, size_t data_size)
{
	if (!table || !key || key_size == 0)
	{
		errno = EINVAL;
		return -1;
	}
	int err;
	const node_t* curr;
	char* curr_key = NULL;
	size_t hash = table->hash_function((void*) key) % table->buckets_no;
	for (curr = LinkedList_GetFirst((table->buckets)[hash]); curr != NULL; curr = Node_GetNext(curr))
	{
		err = Node_CopyKey(curr, &curr_key);
		if (err != 0) return -1;
		if (table->hash_compare(key, curr_key) == 0) // duplicates are not allowed
		{
			free(curr_key);
			return 0;
		}
		free(curr_key);
	}
	err = LinkedList_PushBack((table->buckets)[hash], key, key_size, data, data_size);
	if (err != 0) return -1; // errno is now ENOMEM
	return 1;
}

int
HashTable_Find(const hashtable_t* table, const void* key)
{
	if (!table || !key)
	{
		errno = EINVAL;
		return -1;
	}
	size_t hash = table->hash_function((void*) key) % table->buckets_no;
	const node_t* curr;
	char* curr_key;
	int err;
	for (curr = LinkedList_GetFirst((table->buckets)[hash]); curr != NULL; curr = Node_GetNext(curr))
	{
		err = Node_CopyKey(curr, &curr_key);
		if (err != 0) return -1;
		if (table->hash_compare(key, curr_key) == 0)
		{
			free(curr_key);
			return 1;
		}
		free(curr_key);
	}
	return 0;
}

size_t
HashTable_CopyOutData(const hashtable_t* table, const void* key, void** dataptr)
{
	if (!table || !key || !dataptr)
	{
		errno = EINVAL;
		return 0;
	}
	size_t hash = table->hash_function((void*) key) % table->buckets_no;
	const node_t* curr;
	char* curr_key;
	size_t size;
	int err;
	*dataptr = NULL;
	for (curr = LinkedList_GetFirst((table->buckets)[hash]); curr != NULL; curr = Node_GetNext(curr))
	{
		err = Node_CopyKey(curr, &curr_key);
		if (err != 0) return -1;
		if (table->hash_compare(key, curr_key) == 0)
		{
			size = Node_CopyData(curr, dataptr);
			free(curr_key);
			return size;
		}
		free(curr_key);
	}
	return 0;
}

const void*
HashTable_GetPointerToData(const hashtable_t* table, const void* key)
{
	if (!table || !key)
	{
		errno = EINVAL;
		return NULL;
	}
	size_t hash = table->hash_function((void*) key) % table->buckets_no;
	const node_t* curr;
	char* curr_key;
	int err;
	const void* tmp;
	for (curr = LinkedList_GetFirst((table->buckets)[hash]); curr != NULL; curr = Node_GetNext(curr))
	{
		err = Node_CopyKey(curr, &curr_key);
		if (err != 0) return NULL;
		if (table->hash_compare(key, curr_key) == 0)
		{
			tmp = Node_GetData(curr);
			free(curr_key);
			return tmp;
		}
		free(curr_key);
	}
	errno = ENOENT;
	return NULL;
}

int
HashTable_DeleteNode(hashtable_t* table, const void* key)
{
	if (!table || !key)
	{
		errno = EINVAL;
		return -1;
	}
	size_t hash = table->hash_function((void*) key) % table->buckets_no;
	return LinkedList_Remove((table->buckets)[hash], key);
}

void
HashTable_Free(hashtable_t* table)
{
	if (table)
	{
		for (size_t i = 0; i < table->buckets_no; i++)
		{
			LinkedList_Free((table->buckets)[i]);
		}
		free(table->buckets);
		free(table);
	}
	return;
}

void
HashTable_Print(const hashtable_t* table)
{
	if (!table)
	{
		fprintf(stdout, "NULL\n");
		return;
	}
	for (size_t i = 0; i < table->buckets_no; i++)
	{
		fprintf(stdout, "BUCKET NO. %lu:\n", i);
		LinkedList_Print((table->buckets)[i]);
	}
	return;
}