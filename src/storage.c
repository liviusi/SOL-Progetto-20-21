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
#include <server_defines.h>
#include <wrappers.h>
#include <linked_list.h>
#include <hashtable.h>
#include <storage.h>

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

	// metadata to avoid race conditions
	int lock_owner; // lock owner's fd; when there is none, it is set to 0.
	linked_list_t* pending_locks; // list of fds waiting for the lock on this file to be released.
	linked_list_t* called_open; // list of fds which called open on this file

	// whoever called open on this file with O_CREATE and O_LOCK may call write on this file
	int potential_writer; // will be set to 0 if there is none
	/**
	 * to allow for simultaneous reading operations while no writing operation
	 * is going on two mutexes are needed; if a reading operation is going on
	 * the only lock to be held is the reading one, while a writing operation
	 * such as writeFile, appendToFile, etc both are needed.
	*/
	pthread_mutex_t writing; // lock used to ensure at most one writer at a time
	pthread_mutex_t reading; // lock used to allow for multiple readers
	pthread_cond_t cond;
	bool flag; // toggled on when it is being written
	unsigned int readers_num;

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
	linked_list_t* tmp_pending_locks = NULL;
	linked_list_t* tmp_called_open = NULL;
	bool writing_initialized = false, reading_initialized = false, cond_initialized = false;
	int err;

	#define DEALLOCATE_MUTEX_AND_COND \
		if (writing_initialized) pthread_mutex_destroy(&tmp_writing); \
		if (reading_initialized) pthread_mutex_destroy(&tmp_reading); \
		if (cond_initialized) pthread_cond_destroy(&tmp_cond);

	pthread_mutex_t tmp_writing;
	err = pthread_mutex_init(&tmp_writing, NULL);
	if (err != 0) goto mutex_cond_failure;
	else writing_initialized = true;
	
	pthread_mutex_t tmp_reading;
	err = pthread_mutex_init(&tmp_reading, NULL);
	if (err != 0) goto mutex_cond_failure;
	else reading_initialized = true;
	
	pthread_cond_t tmp_cond;
	err = pthread_cond_init(&tmp_cond, NULL);
	if (err != 0) goto mutex_cond_failure;
	else cond_initialized = true;

	tmp = (stored_file_t*) malloc(sizeof(stored_file_t));
	CHECK_MALLOC_FAILURE(tmp);
	tmp_name = (char*) malloc(strlen(name) + 1);
	CHECK_MALLOC_FAILURE(tmp_name);
	if (contents_size != 0)
	{
		tmp_contents = (void*) malloc(contents_size);
		CHECK_MALLOC_FAILURE(tmp_contents);
	}
	tmp_pending_locks = LinkedList_Init();
	CHECK_MALLOC_FAILURE(tmp_pending_locks);
	tmp_called_open = LinkedList_Init();
	CHECK_MALLOC_FAILURE(tmp_called_open);

	strncpy(tmp_name, name, strlen(name) + 1);
	memcpy(tmp_contents, contents, contents_size);
	tmp->name = tmp_name;
	tmp->contents = tmp_contents;
	tmp->contents_size = contents_size;
	tmp->lock_owner = 0;
	tmp->pending_locks = tmp_pending_locks;
	tmp->called_open = tmp_called_open;
	tmp->potential_writer = 0;
	tmp->writing = tmp_writing;
	tmp->reading = tmp_reading;
	tmp->flag = false;
	tmp->readers_num = 0;

	return tmp;

	mutex_cond_failure:
		DEALLOCATE_MUTEX_AND_COND;
		return NULL;
	
	no_more_memory:
		free(tmp_name);
		free(tmp_contents);
		LinkedList_Free(tmp_pending_locks);
		LinkedList_Free(tmp_called_open);
		free(tmp);
		DEALLOCATE_MUTEX_AND_COND;
		errno = ENOMEM;
		return NULL;
}

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
	CHECK_MALLOC_FAILURE(tmp);
	tmp_sorted_files = LinkedList_Init();
	CHECK_MALLOC_FAILURE(tmp_sorted_files);
	tmp_files = HashTable_Init(max_files_no, NULL, NULL);
	CHECK_MALLOC_FAILURE(tmp_files);

	tmp->algorithm = chosen_algo;
	tmp->files = tmp_files;
	tmp->sorted_files = tmp_sorted_files;
	tmp->mutex = tmp_mutex;
	tmp->max_files_no = max_files_no;
	tmp->max_storage_size = max_storage_size;
	tmp->storage_size = 0;
	tmp->files_no = 0;

	return tmp;

	no_more_memory:
		pthread_mutex_destroy(&tmp_mutex); // it cannot fail
		LinkedList_Free(tmp_sorted_files);
		HashTable_Free(tmp_files);
		free(tmp);
		errno = ENOMEM;
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
	return; // TODO: free allocated resources
}

int
Storage_openFile(storage_t* storage, const char* filename, int flags, int client)
{
	if (!storage || !filename)
	{
		errno = EINVAL;
		return OP_FAILURE;
	}
	int err, exists;
	stored_file_t* file;
	char str_client[BUFFERLEN];
	int len = snprintf(str_client, BUFFERLEN, "%d", client);

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
		if (storage->files_no == storage->max_files_no)
		{
			// should do some proper file deletion
			return OP_FAILURE;
		}
		else
		{
			storage->files_no++;
			file = StoredFile_Init(filename, NULL, 0);
			if (!file) goto fatal;
			if (IS_O_LOCK_SET(flags)) file->lock_owner = client;
			if (IS_O_LOCK_SET(flags) && IS_O_CREATE_SET(flags)) file->potential_writer = client;
			err = LinkedList_PushFront(file->called_open, str_client, len+1, NULL, 0);
			if (err == -1 && errno == ENOMEM) goto fatal;
			err = HashTable_Insert(storage->files, (void*) filename,
						strlen(filename) + 1, (void*) file, sizeof(*file));
			if (err == -1 && errno == ENOMEM) goto fatal;
			if (storage->algorithm == FIFO)
			{
				err = LinkedList_PushFront(storage->sorted_files, filename, strlen(filename) + 1, NULL, 0);
				if (err == -1 && errno == ENOMEM) goto fatal;
			}
		}
	}
	else // file is already inside the storage
	{
		// forcing it not to be const as it needs to be edited
		file = (stored_file_t*) HashTable_GetPointerToData(storage->files, (void*) filename);

		// file needs to be edited in read/write mode
		RETURN_FATAL_IF_NEQ(err, 0, pthread_mutex_lock(&(file->reading)));
		RETURN_FATAL_IF_NEQ(err, 0, pthread_mutex_lock(&(file->writing)));

		if (IS_O_LOCK_SET(flags))
		{
			if (file->lock_owner != client) file->lock_owner = client;
			else
			{
				errno = EACCES;
				return OP_FAILURE;
			}
		}
		// the same client cannot open a file it already opened
		err = LinkedList_Contains(file->called_open, str_client);
		if (err == -1) // ENOMEM
		{
			RETURN_FATAL_IF_NEQ(err, 0, pthread_mutex_unlock(&(file->reading)));
			RETURN_FATAL_IF_NEQ(err, 0, pthread_mutex_unlock(&(file->writing)));
			goto fatal;
		}
		else if (err == 1) // client has already opened this file
		{
			RETURN_FATAL_IF_NEQ(err, 0, pthread_mutex_unlock(&(file->reading)));
			RETURN_FATAL_IF_NEQ(err, 0, pthread_mutex_unlock(&(file->writing)));
			RETURN_FATAL_IF_NEQ(err, 0, pthread_mutex_unlock(&(storage->mutex)));
			return OP_FAILURE;
		}
		else LinkedList_PushFront(file->called_open, str_client, len+1, NULL, 0);

		RETURN_FATAL_IF_NEQ(err, 0, pthread_mutex_unlock(&(file->reading)));
		RETURN_FATAL_IF_NEQ(err, 0, pthread_mutex_unlock(&(file->writing)));
	}

	RETURN_FATAL_IF_NEQ(err, 0, pthread_mutex_unlock(&(storage->mutex)));
	return OP_SUCCESS;

	fatal:
		RETURN_FATAL_IF_NEQ(err, 0, pthread_mutex_unlock(&(storage->mutex)));
		return OP_FATAL;
}

int
Storage_closeFile(storage_t* storage, const char* filename, int client)
{
	if (!storage || !filename)
	{
		errno = EINVAL;
		return OP_FAILURE;
	}

	int err, exists;
	stored_file_t* file;
	char str_client[BUFFERLEN];
	int len = snprintf(str_client, BUFFERLEN, "%d", client);
	RETURN_FATAL_IF_NEQ(err, 0, pthread_mutex_lock(&(storage->mutex)));
	exists = HashTable_Find(storage->files, (void*) filename);
	if (!exists) // file is not inside the storage
		return OP_FAILURE;
	else // file is inside the storage
	{
		// forcing it not to be const as it needs to be edited
		file = (stored_file_t*) HashTable_GetPointerToData(storage->files, (void*) filename);

		// file needs to be edited in read/write mode
		RETURN_FATAL_IF_NEQ(err, 0, pthread_mutex_lock(&(file->reading)));
		RETURN_FATAL_IF_NEQ(err, 0, pthread_mutex_lock(&(file->writing)));

		while (file->flag || file->readers_num) // there must be no writers and no readers
		{
			RETURN_FATAL_IF_NEQ(err, 0, pthread_cond_wait(&(file->cond), &(file->writing)));
		}

		RETURN_FATAL_IF_NEQ(err, 0, pthread_mutex_unlock(&(file->reading)));
		RETURN_FATAL_IF_NEQ(err, 0, pthread_mutex_unlock(&(file->writing)));
	}

	return OP_SUCCESS;
}