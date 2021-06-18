/**
 * @brief Source file for storage header. 
 * IT IS TO BE REDONE FROM SCRATCH.
 * @author Giacomo Trapani.
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>

#include <rwlock.h>
#include <storage.h>
#include <hashtable.h>
#include <linked_list.h>
#include <server_defines.h>
#include <wrappers.h>


#define BUFFERLEN 256
#define DEBUG

#ifdef DEBUG
#include <linked_list.h>
#endif

typedef struct _stored_file
{
	// actual data
	char* name;
	void* contents;
	size_t contents_size;

	int lock_owner; // lock owner's fd; when there is none, it is set to 0.
	linked_list_t* called_open; // list of fds which called open on this file

	// whoever called open on this file with O_CREATE and O_LOCK may call write on this file
	int potential_writer; // will be set to 0 if there is none
	rwlock_t* lock;

} stored_file_t;

stored_file_t*
StoredFile_Init(const char* name, const void* contents, size_t contents_size)
{
	if (!name)
	{
		errno = EINVAL;
		return NULL;
	}
	stored_file_t* tmp = NULL;
	char* tmp_name = NULL;
	void* tmp_contents = NULL;
	linked_list_t* tmp_called_open = NULL;
	rwlock_t* tmp_lock = NULL;
	int err;

	tmp = (stored_file_t*) malloc(sizeof(stored_file_t));
	GOTO_LABEL_IF_EQ(tmp, NULL, err, init_failure);
	tmp_name = (char*) malloc(strlen(name) + 1);
	GOTO_LABEL_IF_EQ(tmp_name, NULL, err, init_failure);
	if (contents_size != 0)
	{
		tmp_contents = (void*) malloc(contents_size);
		GOTO_LABEL_IF_EQ(tmp_contents, NULL, err, init_failure);
	}
	tmp_called_open = LinkedList_Init(free);
	GOTO_LABEL_IF_EQ(tmp_called_open, NULL, err, init_failure);
	tmp_lock = RWLock_Init();
	GOTO_LABEL_IF_EQ(tmp_lock, NULL, err, init_failure);

	strncpy(tmp_name, name, strlen(name) + 1);
	memcpy(tmp_contents, contents, contents_size);
	tmp->name = tmp_name;
	tmp->contents = tmp_contents;
	tmp->contents_size = contents_size;
	tmp->lock_owner = 0;
	tmp->called_open = tmp_called_open;
	tmp->potential_writer = 0;
	tmp->lock = tmp_lock;

	return tmp;

	init_failure:
		free(tmp_name);
		free(tmp_contents);
		LinkedList_Free(tmp_called_open);
		RWLock_Free(tmp_lock);
		free(tmp);
		errno = err;
		return NULL;
}

void
StoredFile_Free(void* arg)
{
	stored_file_t* file = (stored_file_t*) arg;
	RWLock_Free(file->lock);
	LinkedList_Free(file->called_open);
	free(file->name);
	free(file->contents);
	free(file);
}

#ifndef DEBUG
struct _storage
{
	hashtable_t* files;
	replacement_algo_t algorithm; // right now only FIFO is to be supported.
	linked_list_t* sorted_files;

	size_t max_files_no;
	size_t max_storage_size;
	size_t files_no;
	size_t storage_size;

	pthread_mutex_t mutex;
};
#endif

storage_t*
Storage_Init(size_t max_files_no, size_t max_storage_size, replacement_algo_t chosen_algo)
{
	if (max_files_no == 0 || max_storage_size == 0)
	{
		errno = EINVAL;
		return NULL;
	}
	storage_t* tmp = NULL;
	linked_list_t* tmp_sorted_files = NULL;
	hashtable_t* tmp_files = NULL;
	pthread_mutex_t tmp_mutex;
	int err;
	err = pthread_mutex_init(&tmp_mutex, NULL);
	if (err != 0) return NULL;
	tmp = (storage_t*) malloc(sizeof(storage_t));
	GOTO_LABEL_IF_EQ(tmp, NULL, err, init_failure);
	tmp_sorted_files = LinkedList_Init(free);
	GOTO_LABEL_IF_EQ(tmp_sorted_files, NULL, err, init_failure);
	tmp_files = HashTable_Init(max_files_no, NULL, NULL, StoredFile_Free);
	GOTO_LABEL_IF_EQ(tmp_files, NULL, err, init_failure);

	tmp->algorithm = chosen_algo;
	tmp->files = tmp_files;
	tmp->sorted_files = tmp_sorted_files;
	tmp->mutex = tmp_mutex;
	tmp->max_files_no = max_files_no;
	tmp->max_storage_size = max_storage_size;
	tmp->storage_size = 0;
	tmp->files_no = 0;

	return tmp;

	init_failure:
		pthread_mutex_destroy(&tmp_mutex); // it cannot fail
		LinkedList_Free(tmp_sorted_files);
		HashTable_Free(tmp_files);
		free(tmp);
		errno = err;
		return NULL;
}

void
StoredFile_Print(const stored_file_t* file)
{
	printf("\t\tFilename : %s\n", file->name);
	printf("\t\tSize : %lu\n", file->contents_size);
	char* contents = NULL;
	char* tmp;
	const node_t* curr = NULL;
	if (file->contents)
	{
		contents = (char*) malloc(sizeof(char) * (file->contents_size+1));
		if (!contents)
		{
			printf("\nFATAL ERROR OCCURRED WHILE PRINTING FILE\n");
			errno = ENOMEM;
			exit(errno);
		}
		memcpy(contents, file->contents, file->contents_size);
		contents[file->contents_size] = '\0';
		printf("\t\tContents : \n%s\n", contents);
	}
	else printf("\t\tContents: NULL\n");
	if (file->called_open)
	{
		printf("\t\tCalled open: ");
		for (curr = LinkedList_GetFirst(file->called_open); curr != NULL; curr = Node_GetNext(curr))
		{
			Node_CopyKey(curr, &tmp);
			printf("%s -> ", tmp);
			free(tmp);
		}
		printf("NULL");
	}
	else printf("\t\tCalled open: NULL\n");
	free(contents);
}

#ifdef DEBUG
void
Storage_Print(const storage_t* storage)
{
	printf("STORAGE DETAILS\n");
	printf("\tCurrent files : %lu, Maximum : %lu\n",
			storage->files_no, storage->max_files_no);
	printf("\tCurrent size : %lu, Maximum : %lu\n",
			storage->storage_size, storage->max_storage_size);
	const node_t* curr;
	const stored_file_t* tmp;
	for (size_t i = 0; i < storage->max_files_no; i++)
	{
		printf("\tBucket no. %lu\n", i);
		curr = LinkedList_GetFirst((storage->files)->buckets[i]);
		if (!curr) printf("\t\tEMPTY\n");
		for (; curr != NULL; curr = Node_GetNext(curr))
		{
			tmp = (const stored_file_t*) Node_GetData(curr);
			StoredFile_Print(tmp);
		}
		printf("\n");
	}
	return;
}

#else
void
Storage_Print(const storage_t* storage)
{
	return;
}

#endif

void
Storage_Free(storage_t* storage)
{
	if (!storage) return;
	pthread_mutex_destroy(&(storage->mutex));
	LinkedList_Free(storage->sorted_files);
	HashTable_Free(storage->files);
	free(storage);
}

int
Storage_openFile(storage_t* storage, const char* filename, int flags, int client)
{
	if (!storage || !filename)
	{
		errno = EINVAL;
		return OP_FAILURE;
	}
	int err, exists, errnosave;
	stored_file_t* file;

	// converting client to a string
	char str_client[BUFFERLEN];
	int len = snprintf(str_client, BUFFERLEN, "%d", client);

	// acquire mutex over storage
	RETURN_FATAL_IF_NEQ(err, 0, pthread_mutex_lock(&(storage->mutex)));
	// it already exists and flag contains O_CREATE
	if ((exists = HashTable_Find(storage->files, (void*) filename)) == (IS_O_CREATE_SET(flags)))
	{
		RETURN_FATAL_IF_NEQ(err, 0, pthread_mutex_unlock(&(storage->mutex)));
		errno = EPERM;
		return OP_FAILURE;
	}
	if (exists == 0) // file is not inside the storage
	{
		if (storage->files_no == storage->max_files_no ||
				storage->storage_size + sizeof(stored_file_t) > storage->max_storage_size)
		{
			// should do some proper file deletion
			RETURN_FATAL_IF_NEQ(err, 0, pthread_mutex_unlock(&(storage->mutex)));
			return OP_FAILURE;
		}
		else
		{
			storage->files_no++;
			storage->storage_size += sizeof(stored_file_t);
			file = StoredFile_Init(filename, NULL, 0);
			GOTO_LABEL_IF_EQ(file, NULL, errnosave, fatal); // file could not be created
			if (IS_O_LOCK_SET(flags)) file->lock_owner = client;
			if (IS_O_LOCK_SET(flags) && IS_O_CREATE_SET(flags)) file->potential_writer = client;
			err = LinkedList_PushFront(file->called_open, str_client, len+1, NULL, 0);
			GOTO_LABEL_IF_EQ(err, -1, errnosave, fatal);
			err = HashTable_Insert(storage->files, (void*) filename,
						strlen(filename) + 1, (void*) file, sizeof(*file));
			GOTO_LABEL_IF_EQ(err, -1, errnosave, fatal);
			if (storage->algorithm == FIFO)
			{
				err = LinkedList_PushFront(storage->sorted_files, filename, strlen(filename) + 1, NULL, 0);
				GOTO_LABEL_IF_EQ(err, -1, errnosave, fatal);
			}
			// file has been copied inside storage
			// it is to be freed
			free(file);
		}
	}
	else // file is already inside the storage
	{
		// forcing it not to be const as it needs to be edited
		file = (stored_file_t*) HashTable_GetPointerToData(storage->files, (void*) filename);

		// file needs to be edited in write mode
		RETURN_FATAL_IF_NEQ(err, 0, RWLock_WriteLock(file->lock));

		// the same client cannot open a file it already opened
		err = LinkedList_Contains(file->called_open, str_client);
		if (err == -1) // ENOMEM
		{
			RETURN_FATAL_IF_NEQ(err, 0, RWLock_WriteUnlock(file->lock));
			errnosave = ENOMEM;
			goto fatal;
		}
		else if (err == 1) // client has already opened this file
		{
			if (IS_O_LOCK_SET(flags))
			{
				if (file->lock_owner == 0) file->lock_owner = client;
				else // someone already owns this file's lock
				{
					RETURN_FATAL_IF_NEQ(err, 0, RWLock_WriteUnlock(file->lock));
					RETURN_FATAL_IF_NEQ(err, 0, pthread_mutex_unlock(&(storage->mutex)));
					errno = EACCES;
					return OP_FAILURE;
				}
			}
			RETURN_FATAL_IF_NEQ(err, 0, RWLock_WriteUnlock(file->lock));
			RETURN_FATAL_IF_NEQ(err, 0, pthread_mutex_unlock(&(storage->mutex)));
			errno = EBADF;
			return OP_FAILURE;
		}
		else
		{
			// add the client to the list of the ones who opened this file
			err = LinkedList_PushFront(file->called_open, str_client, len+1, NULL, 0);
			if (err != 0)
			{
				err = RWLock_WriteUnlock(file->lock);
				GOTO_LABEL_IF_NEQ(err, 0, errnosave, fatal);
				errnosave = ENOMEM;
				goto fatal;
			}
		}

		RETURN_FATAL_IF_NEQ(err, 0, RWLock_WriteUnlock(file->lock));
	}

	RETURN_FATAL_IF_NEQ(err, 0, pthread_mutex_unlock(&(storage->mutex)));
	return OP_SUCCESS;

	fatal:
		RETURN_FATAL_IF_NEQ(err, 0, pthread_mutex_unlock(&(storage->mutex)));
		errno = errnosave;
		return OP_FATAL;
}

int
Storage_readFile(storage_t* storage, const char* filename, void** buf, size_t* size, int client)
{
	if (!storage || !filename || !buf || !size)
	{
		errno = EINVAL;
		return OP_FAILURE;
	}
	int err, exists;

	char str_client[BUFFERLEN];
	stored_file_t* file;
	void* tmp_contents = NULL;
	size_t tmp_size = 0;
	snprintf(str_client, BUFFERLEN, "%d", client);
	RETURN_FATAL_IF_NEQ(err, 0, pthread_mutex_lock(&(storage->mutex)));
	exists = HashTable_Find(storage->files, (void*) filename); // cannot fail
	if (exists == 1) // file is inside the storage
	{
		file = (stored_file_t*) HashTable_GetPointerToData(storage->files, (void*) filename);

		RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadLock(file->lock));
		if (file->lock_owner != 0 && file->lock_owner != client)
		{
			RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadUnlock(file->lock));
			*buf = NULL;
			*size = 0;
			errno = EACCES;
			return OP_FAILURE;
		}
		err = LinkedList_Contains(file->called_open, str_client);
		if (err == 0) // file has not been opened by this client
		{
			RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadUnlock(file->lock));
			*buf = NULL;
			*size = 0;
			return OP_FAILURE;
		}
		else if (err == -1) // an error occurred
		{
			RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadUnlock(file->lock));
			RETURN_FATAL_IF_NEQ(err, 0, pthread_mutex_unlock(&(storage->mutex)));
			*buf = NULL;
			*size = 0;
			return OP_FATAL;
		}
		else // file has been opened by this client
		{
			if (file->contents_size == 0 || !file->contents)
			{
				*buf = NULL;
				*size = 0;
				RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadUnlock(file->lock));
				RETURN_FATAL_IF_NEQ(err, 0, pthread_mutex_unlock(&(storage->mutex)));
				return OP_SUCCESS;
			}
			else
			{
				tmp_size = file->contents_size;
				tmp_contents = malloc(tmp_size);
				memcpy(tmp_contents, file->contents, tmp_size);
				RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadUnlock(file->lock));

				RETURN_FATAL_IF_NEQ(err, 0, RWLock_WriteLock(file->lock));
				file->potential_writer = 0;
				RETURN_FATAL_IF_NEQ(err, 0, RWLock_WriteUnlock(file->lock));
			}
		}
	}
	else
	{
		*size = tmp_size;
		*buf = tmp_contents;
		errno = EBADF;
		return OP_FAILURE;
	}
	RETURN_FATAL_IF_NEQ(err, 0, pthread_mutex_unlock(&(storage->mutex)));
	*size = tmp_size;
	*buf = tmp_contents;
	return OP_SUCCESS;
}

int
Storage_closeFile(storage_t* storage, const char* filename, int client)
{
	if (!storage || !filename)
	{
		errno = EINVAL;
		return OP_FAILURE;
	}

	int err, exists, errnosave;
	stored_file_t* file;
	char str_client[BUFFERLEN];
	snprintf(str_client, BUFFERLEN, "%d", client);
	RETURN_FATAL_IF_NEQ(err, 0, pthread_mutex_lock(&(storage->mutex)));
	exists = HashTable_Find(storage->files, (void*) filename);
	if (!exists) // file is not inside the storage
	{
		RETURN_FATAL_IF_NEQ(err, 0, pthread_mutex_unlock(&(storage->mutex)));
		errno = EBADF;
		return OP_FAILURE;
	}
	else // file is inside the storage
	{
		// forcing it not to be const as it needs to be edited
		file = (stored_file_t*) HashTable_GetPointerToData(storage->files, (void*) filename);

		// file needs to be edited in write mode
		RETURN_FATAL_IF_NEQ(err, 0, RWLock_WriteLock(file->lock));

		// checking whether client has opened the file
		err = LinkedList_Contains(file->called_open, str_client);
		if (err == -1)
		{
			errnosave = errno;
			RETURN_FATAL_IF_NEQ(err, 0, RWLock_WriteUnlock((file->lock)));
			RETURN_FATAL_IF_NEQ(err, 0, pthread_mutex_unlock(&(storage->mutex)));
			errno = errnosave;
			return OP_FATAL;
		}
		if (err == 0) // file has not been opened by client
		{
			RETURN_FATAL_IF_NEQ(err, 0, RWLock_WriteUnlock((file->lock)));
			RETURN_FATAL_IF_NEQ(err, 0, pthread_mutex_unlock(&(storage->mutex)));
			errno = EACCES;
			return OP_FAILURE;
		}
		else // file has been opened: now closing it
		{
			LinkedList_Remove(file->called_open, str_client);
		}

		RETURN_FATAL_IF_NEQ(err, 0, RWLock_WriteUnlock(file->lock));
	}

	RETURN_FATAL_IF_NEQ(err, 0, pthread_mutex_unlock(&(storage->mutex)));

	return OP_SUCCESS;
}