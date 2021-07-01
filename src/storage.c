/**
 * @brief Source file for storage header. 
 * @author Giacomo Trapani.
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

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
	char* name; // file name
	void* contents; // file contents
	size_t contents_size; // size of file contents

	int lock_owner; // lock owner's fd; when there is none, it is set to 0.
	linked_list_t* called_open; // list of fds which called open on this file

	// whoever called open on this file with O_CREATE and O_LOCK may call write on this file
	int potential_writer; // will be set to 0 if there is none
	rwlock_t* rwlock; // used for multithreading purposes
	pthread_mutex_t mutex_locked; // mutex used to wait when calling lockFile
	pthread_cond_t cond_locked; // condition variable used when waiting on lock to be released
} stored_file_t;

/**
 * @brief Function to initialize file in storage.
 * @param name cannot be NULL.
 * @returns Initialized data structure on success,
 * NULL on failure.
 * @exception It sets "errno" to "EINVAL" if any param is not valid.
 * The function may fail and set "errno" for any of the errors
 * specified for the routines "malloc", "LinkedList_Init",
 * "RWLock_Init", "pthread_mutex_init", "pthread_cond_init".
*/
static stored_file_t*
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
	pthread_mutex_t tmp_mutex;
	pthread_cond_t tmp_cond;
	bool cond_initialized = false;
	int err, errnosave;

	pthread_mutex_init(&tmp_mutex, NULL);
	err = pthread_cond_init(&tmp_cond, NULL);
	GOTO_LABEL_IF_NEQ(err, 0, errnosave, init_failure);
	cond_initialized = true;
	tmp = (stored_file_t*) malloc(sizeof(stored_file_t));
	GOTO_LABEL_IF_EQ(tmp, NULL, errnosave, init_failure);
	tmp_name = (char*) malloc(strlen(name) + 1);
	GOTO_LABEL_IF_EQ(tmp_name, NULL, errnosave, init_failure);
	if (contents_size != 0 && contents)
	{
		tmp_contents = (void*) malloc(contents_size);
		GOTO_LABEL_IF_EQ(tmp_contents, NULL, errnosave, init_failure);
	}
	tmp_called_open = LinkedList_Init(free);
	GOTO_LABEL_IF_EQ(tmp_called_open, NULL, errnosave, init_failure);
	tmp_lock = RWLock_Init();
	GOTO_LABEL_IF_EQ(tmp_lock, NULL, errnosave, init_failure);

	strncpy(tmp_name, name, strlen(name) + 1);
	memcpy(tmp_contents, contents, contents_size);
	tmp->name = tmp_name;
	tmp->contents = tmp_contents;
	tmp->contents_size = contents_size;
	tmp->lock_owner = 0;
	tmp->called_open = tmp_called_open;
	tmp->potential_writer = 0;
	tmp->rwlock = tmp_lock;
	tmp->mutex_locked = tmp_mutex;
	tmp->cond_locked = tmp_cond;

	return tmp;

	init_failure:
		pthread_mutex_destroy(&tmp_mutex);
		if (cond_initialized) pthread_cond_destroy(&tmp_cond);
		free(tmp_name);
		free(tmp_contents);
		LinkedList_Free(tmp_called_open);
		RWLock_Free(tmp_lock);
		free(tmp);
		errno = errnosave;
		return NULL;
}
/**
 * Utility print function.
*/
static void
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
		printf("NULL\n");
	}
	else printf("\t\tCalled open: NULL\n");
	free(contents);
	printf("\t\tLock owner = %d\n", file->lock_owner);
	printf("\t\tPotential writer = %d\n\n", file->potential_writer);
}

/**
 * Frees allocated resources.
*/
static void
StoredFile_Free(void* arg)
{
	stored_file_t* file = (stored_file_t*) arg;
	RWLock_Free(file->rwlock);
	LinkedList_Free(file->called_open);
	free(file->name);
	free(file->contents);
	free(file);
}

#ifndef DEBUG
struct _storage
{
	hashtable_t* files; // table of files in storage
	replacement_algo_t algorithm; // right now only FIFO is to be supported.
	linked_list_t* sorted_files; // files sorted following replacement policy (sorting) method

	size_t max_files_no; // maximum number of storeable files
	size_t max_storage_size; // maximum storage size
	size_t files_no; // current number of files
	size_t storage_size; // current storage size

	rwlock_t* lock; // used for multithreading purposes
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
	int err;
	storage_t* tmp = NULL;
	linked_list_t* tmp_sorted_files = NULL;
	hashtable_t* tmp_files = NULL;
	rwlock_t* tmp_lock = NULL;
	tmp_lock = RWLock_Init();
	GOTO_LABEL_IF_EQ(tmp_lock, NULL, err, init_failure);
	tmp = (storage_t*) malloc(sizeof(storage_t));
	GOTO_LABEL_IF_EQ(tmp, NULL, err, init_failure);
	tmp_sorted_files = LinkedList_Init(free);
	GOTO_LABEL_IF_EQ(tmp_sorted_files, NULL, err, init_failure);
	tmp_files = HashTable_Init(max_files_no, NULL, NULL, StoredFile_Free);
	GOTO_LABEL_IF_EQ(tmp_files, NULL, err, init_failure);

	tmp->algorithm = chosen_algo;
	tmp->files = tmp_files;
	tmp->sorted_files = tmp_sorted_files;
	tmp->lock = tmp_lock;
	tmp->max_files_no = max_files_no;
	tmp->max_storage_size = max_storage_size;
	tmp->storage_size = 0;
	tmp->files_no = 0;

	return tmp;

	init_failure:
		err = errno;
		RWLock_Free(tmp_lock); // it cannot fail
		LinkedList_Free(tmp_sorted_files);
		HashTable_Free(tmp_files);
		free(tmp);
		errno = err;
		return NULL;
}

/**
 * @brief Gets victim name from storage.
 * @returns 0 on success, -1 on failure.
 * @param storage cannot be NULL.
 * @param victim_name cannot be NULL.
 * @exception It sets "errno" to "EINVAL" if any param is not valid.
 * The function may also fail and set "errno" for any of the errors
 * specified for the routine "LinkedList_PopBack". 
*/
static int
Storage_getVictim(storage_t* storage, char** victim_name)
{
	if (!victim_name || !storage)
	{
		errno = EINVAL;
		return -1;
	}
	errno = 0;
	size_t res = LinkedList_PopBack(storage->sorted_files, victim_name, NULL);
	if (res == 0 && errno == ENOMEM) return -1;
	return 0;
}

int
Storage_openFile(storage_t* storage, const char* pathname, int flags, int client)
{
	if (!storage || !pathname)
	{
		errno = EINVAL;
		return OP_FAILURE;
	}

	int err, exists;
	stored_file_t* file;
	char str_client[BUFFERLEN]; // used to denote client as a string
	int len = snprintf(str_client, BUFFERLEN, "%d", client); // converting client to a string

	// acquire mutex over storage
	RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadLock(storage->lock));
	// check whether file exists
	RETURN_FATAL_IF_EQ(exists, -1, HashTable_Find(storage->files, (void*) pathname));

	if ((exists == 1) && (IS_O_CREATE_SET(flags))) // file exists, creation flag is set
	{
		RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadUnlock(storage->lock));
		errno = EEXIST;
		return OP_FAILURE;
	}
	if (exists == 0) // file is not inside the storage
	{
		if (!(IS_O_CREATE_SET(flags)))
		{
			RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadUnlock(storage->lock));
			errno = ENOENT;
			return OP_FAILURE;
		}
		if (storage->files_no == storage->max_files_no)
		{
			RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadUnlock(storage->lock));
			errno = ENOSPC;
			return OP_FAILURE;
		}
		else // add file to storage
		{
			storage->files_no++;
			RETURN_FATAL_IF_EQ(file, NULL, StoredFile_Init(pathname, NULL, 0));
			if (IS_O_LOCK_SET(flags))
				file->lock_owner = client; // client owns this file's lock
			if (IS_O_LOCK_SET(flags) && IS_O_CREATE_SET(flags))
				file->potential_writer = client; // client can write this file
			RETURN_FATAL_IF_NEQ(err, 0, LinkedList_PushFront(file->called_open, str_client,
						len+1, NULL, 0));
			RETURN_FATAL_IF_EQ(err, -1, HashTable_Insert(storage->files, (void*) pathname,
						strlen(pathname) + 1, (void*) file, sizeof(*file)));
			if (storage->algorithm == FIFO)
			{
				RETURN_FATAL_IF_EQ(err, -1, LinkedList_PushFront(storage->sorted_files, 
						pathname, strlen(pathname) + 1, NULL, 0));
			}
			// file has been copied inside storage
			// it is to be freed
			free(file);
		}
	}
	else // file is already inside the storage
	{
		// forcing it not to be const as it needs to be edited
		RETURN_FATAL_IF_EQ(file, NULL, (stored_file_t*)
					HashTable_GetPointerToData(storage->files, (void*) pathname));
		// acquire file lock
		RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadLock(file->rwlock));
		// the same client cannot open a file it has already opened
		RETURN_FATAL_IF_EQ(err, -1, LinkedList_Contains(file->called_open, str_client));
		if (err == 1) // client has already opened this file
		{
			RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadUnlock(file->rwlock));
			RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadUnlock(storage->lock));
			errno = EBADF;
			return OP_FAILURE;
		}
		else
		{
			if (IS_O_LOCK_SET(flags)) // client wants to acquire lock
			{
				if (file->lock_owner == 0) // no client is already holding it
				{
					RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadUnlock(file->rwlock));
					RETURN_FATAL_IF_NEQ(err, 0, RWLock_WriteLock(file->rwlock));
					file->lock_owner = client;
					RETURN_FATAL_IF_NEQ(err, 0, RWLock_WriteUnlock(file->rwlock));
				}
				else // a client already owns this file's lock
				{
					RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadUnlock(file->rwlock));
					RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadUnlock(storage->lock));
					errno = EACCES;
					return OP_FAILURE;
				}
			}
			else RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadUnlock(file->rwlock));
			RETURN_FATAL_IF_NEQ(err, 0, RWLock_WriteLock(file->rwlock));
			// add the client to the list of the ones who opened this file
			RETURN_FATAL_IF_NEQ(err, 0 , LinkedList_PushFront(file->called_open,
						str_client, len+1, NULL, 0));
			RETURN_FATAL_IF_NEQ(err, 0, RWLock_WriteUnlock(file->rwlock));
		}

	}

	RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadUnlock(storage->lock));
	return OP_SUCCESS;
}

int
Storage_readFile(storage_t* storage, const char* pathname, void** buf, size_t* size, int client)
{
	if (!storage || !pathname || !buf || !size)
	{
		errno = EINVAL;
		return OP_FAILURE;
	}

	int err, exists;
	char str_client[BUFFERLEN]; // used to denote client as a string
	stored_file_t* file;
	void* tmp_contents = NULL; // used to denote file contents
	size_t tmp_size = 0; // used to denote file size
	*buf = NULL; *size = 0; // initializing both params to improve readability
	snprintf(str_client, BUFFERLEN, "%d", client); // convert int client to string
	RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadLock(storage->lock));
	RETURN_FATAL_IF_EQ(exists, -1, HashTable_Find(storage->files, (void*) pathname));

	if (exists == 1) // file is inside the storage
	{
		RETURN_FATAL_IF_EQ(file, NULL, (stored_file_t*)
					HashTable_GetPointerToData(storage->files, (void*) pathname));
		RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadLock(file->rwlock));
		// a client already owns this file's lock
		if (file->lock_owner != 0 && file->lock_owner != client)
		{
			RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadUnlock(file->rwlock));
			RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadUnlock(storage->lock));
			errno = EACCES;
			return OP_FAILURE;
		}
		RETURN_FATAL_IF_EQ(err, -1, LinkedList_Contains(file->called_open, str_client));
		if (err == 0) // file has not been opened by this client
		{
			RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadUnlock(file->rwlock));
			RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadUnlock(storage->lock));
			errno = EACCES;
			return OP_FAILURE;
		}
		else // file has been opened by this client
		{
			// file is empty
			if (file->contents_size == 0 || !file->contents)
			{
				RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadUnlock(file->rwlock));
				RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadUnlock(storage->lock));
				return OP_SUCCESS;
			}
			else // file is not empty
			{
				tmp_size = file->contents_size;
				RETURN_FATAL_IF_EQ(tmp_contents, NULL, malloc(tmp_size));
				memcpy(tmp_contents, file->contents, tmp_size);
				RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadUnlock(file->rwlock));
				RETURN_FATAL_IF_NEQ(err, 0, RWLock_WriteLock(file->rwlock));
				file->potential_writer = 0; // writing this file is not allowed
				RETURN_FATAL_IF_NEQ(err, 0, RWLock_WriteUnlock(file->rwlock));
				RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadUnlock(storage->lock));

			}
		}
	}
	else // file is not inside the storage
	{
		RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadUnlock(storage->lock));
		errno = EBADF;
		return OP_FAILURE;
	}
	// actual assignment
	*size = tmp_size;
	*buf = tmp_contents;
	return OP_SUCCESS;
}

/**
 * @brief Checks whether file is a regular file.
 * @returns 1 if it is a regular file, 0 if it is not, -1 on failure.
 * @param pathname cannot be NULL.
 * @exception It sets "errno" to "EINVAL" if any param is not valid.
 * The function may also fail and set "errno" for any of the errors
 * specified for the routine "stat".
*/
int is_regular_file(const char *pathname)
{
	if (!pathname)
	{
		errno = EINVAL;
		return -1;
	}
	struct stat statbuf;
	int err = stat(pathname, &statbuf);
	if (err != 0) return -1;
	if (S_ISREG(statbuf.st_mode))
		return 1;
	return 0;
}

int
Storage_writeFile(storage_t* storage, const char* pathname, linked_list_t** evicted, int client)
{
	if (!storage || !pathname)
	{
		errno = EINVAL;
		return OP_FAILURE;
	}

	int err, exists;
	long length; // buffer length
	long read_length; // actual read length
	bool failure = false; // toggled on if replacement algorithm chooses pathname as a victim
	char* pathname_contents = NULL; // buffer of contents
	char* victim_name = NULL; // used to denote victim's name
	FILE* pathname_file; // used to denote pathname as a FILE*
	stored_file_t* stored_file = NULL; // used to denote pathname as a file inside storage
	stored_file_t* tmp = NULL; // used to denote victim as a file inside storage
	linked_list_t* tmp_evicted = NULL; // list of evicted files

	// must check whether pathname is a regular file
	err = is_regular_file(pathname);
	if (err == -1) return OP_FAILURE;
	if (err == 0)
	{
		errno = EINVAL;
		return OP_FAILURE;
	}

	// opening file
	pathname_file = fopen(pathname, "r");
	if (!pathname_file) return OP_FAILURE;

	// calculating file's content buffer length
	err = fseek(pathname_file, 0, SEEK_END);
	if (err != 0) return OP_FAILURE;
	length = ftell(pathname_file);
	fprintf(stderr, "[%s:%d] %s has length %ld\n", __FILE__, __LINE__, pathname, (unsigned long) length);
	err = fseek(pathname_file, 0, SEEK_SET);
	if (err != 0) return OP_FAILURE;

	// file must not be bigger than storage's size
	if (length > storage->max_storage_size)
	{
		if (evicted) *evicted = tmp_evicted;
		fclose(pathname_file);
		errno = EFBIG;
		return OP_FAILURE;
	}

	// file is not empty
	if (length != 0)
	{
		// +1 for '\0'
		RETURN_FATAL_IF_EQ(pathname_contents, NULL, (char*) malloc(sizeof(char) * (length + 1)));
		read_length = fread(pathname_contents, sizeof(char), length, pathname_file);
		if (read_length != length && ferror(pathname_file)) // fread failed
		{
			if (evicted) *evicted = tmp_evicted;
			fclose(pathname_file);
			free(pathname_contents);
			errno = EBADE;
			return OP_FAILURE;
		}
		pathname_contents[length] = '\0'; // string needs to be null terminated
	}
	fclose(pathname_file);

	// as files may be deleted, no readers are allowed on storage
	RETURN_FATAL_IF_NEQ(err, 0, RWLock_WriteLock(storage->lock));
	RETURN_FATAL_IF_EQ(exists, -1, HashTable_Find(storage->files, (void*) pathname));

	if (exists == 1) // file is inside the storage
	{
		// as there can be at most one writer at a time,
		// no locks need to be acquired over file
		RETURN_FATAL_IF_EQ(stored_file, NULL, (stored_file_t*)
					HashTable_GetPointerToData(storage->files, (void*) pathname));

		if (stored_file->potential_writer != client) // client cannot write this file
		{
			if (evicted) *evicted = tmp_evicted;
			free(pathname_contents);
			RETURN_FATAL_IF_NEQ(err, 0, RWLock_WriteUnlock(storage->lock));
			errno = EACCES;
			return OP_FAILURE;
		}
		// there's no room for this file
		// start replacement algorithm
		if (storage->storage_size + length > storage->max_storage_size)
		{
			if (evicted) // if a list is specified, save evicted files' data
				RETURN_FATAL_IF_EQ(tmp_evicted, NULL, LinkedList_Init(NULL));
			while (!failure)
			{
				if (storage->storage_size + length <= storage->max_storage_size) break;
				RETURN_FATAL_IF_NEQ(err, 0, Storage_getVictim(storage, &victim_name));
				//fprintf(stderr, "[%d] victim is %s\n", __LINE__, victim_name);
				if (strcmp(victim_name, pathname) == 0) // file to be written got evicted
					failure = true;
				RETURN_FATAL_IF_EQ(tmp, NULL, (stored_file_t*)
							HashTable_GetPointerToData(storage->files, (void*) victim_name));
				if (evicted) RETURN_FATAL_IF_NEQ(err, 0, LinkedList_PushFront(tmp_evicted, victim_name,
							strlen(victim_name) + 1, tmp->contents, tmp->contents_size));
				storage->storage_size -= tmp->contents_size; // update storage size
				storage->files_no--; // update number of files
				RETURN_FATAL_IF_NEQ(err, 0, HashTable_DeleteNode(storage->files, (void*) victim_name));
				free(victim_name);
			}
			if (evicted) *evicted = tmp_evicted;
			if (failure) // file to be written got evicted
			{
				free(pathname_contents);
				RETURN_FATAL_IF_NEQ(err, 0, RWLock_WriteUnlock(storage->lock));
				errno = EIDRM;
				return OP_FAILURE;
			}
		}
		if (pathname_contents) // file is not empty
		{
			stored_file->contents_size = length;
			RETURN_FATAL_IF_EQ(stored_file->contents, NULL, malloc(length + 1));
			memcpy(stored_file->contents, pathname_contents, length + 1);
			free(pathname_contents);
		}
		stored_file->potential_writer = 0;
		storage->storage_size+=length;
		RETURN_FATAL_IF_NEQ(err, 0, RWLock_WriteUnlock(storage->lock));
	}
	else
	{
		free(pathname_contents);
		RETURN_FATAL_IF_NEQ(err, 0, RWLock_WriteUnlock(storage->lock));
		errno = EBADF;
		return OP_FAILURE;
	}
	return OP_SUCCESS;
}

int
Storage_appendToFile(storage_t* storage, const char* pathname, void* buf,
			size_t size, linked_list_t** evicted, int client)
{
	if (!storage || !pathname)
	{
		errno = EINVAL;
		return OP_FAILURE;
	}

	int err; // used as a placeholder for functions' return values
	int exists; // set to 1 if file is inside the storage
	bool failure = false; // toggled on when the file the operation should be performed on is a victim.
	char* victim_name; // used to denote victim's name
	stored_file_t* file; // used to denote file in storage corresponding pathname
	stored_file_t* tmp; // used to denote victim(s)
	linked_list_t* tmp_evicted = NULL;
	void* new_contents;
	char str_client[BUFFERLEN]; // used when converting int client to a string

	snprintf(str_client, BUFFERLEN, "%d", client);
	RETURN_FATAL_IF_NEQ(err, 0, RWLock_WriteLock(storage->lock));
	RETURN_FATAL_IF_EQ(exists, -1, HashTable_Find(storage->files, (void*) pathname));

	if (exists == 1) // file is inside the storage
	{
		RETURN_FATAL_IF_EQ(file, NULL, (stored_file_t*)
					HashTable_GetPointerToData(storage->files, (void*) pathname));
		RETURN_FATAL_IF_EQ(err, -1, LinkedList_Contains(file->called_open, str_client));
		// file has not been opened by this client
		if (err == 0)
		{
			RETURN_FATAL_IF_NEQ(err, 0, RWLock_WriteUnlock(storage->lock));
			errno = EACCES;
			return OP_FAILURE;
		}
		// if the lock is held by a different client
		if (file->lock_owner != client && file->lock_owner != 0)
		{
			RETURN_FATAL_IF_NEQ(err, 0, RWLock_WriteUnlock(storage->lock));
			errno = EACCES;
			return OP_FAILURE;
		}
		// there is nothing to append
		if (size == 0 || buf == NULL)
		{
			RETURN_FATAL_IF_NEQ(err, 0, RWLock_WriteUnlock(storage->lock));
			return OP_SUCCESS;
		}
		// handle eventual file deletion
		if (storage->storage_size + size > storage->max_storage_size)
		{
			if (evicted) RETURN_FATAL_IF_EQ(tmp_evicted, NULL, LinkedList_Init(NULL));
			while (!failure)
			{
				if (storage->storage_size + size <= storage->max_storage_size) break;
				RETURN_FATAL_IF_NEQ(err, 0, Storage_getVictim(storage, &victim_name));
				//fprintf(stderr, "victim is %s\n", victim_name);
				if (strcmp(victim_name, pathname) == 0) failure = true;
				RETURN_FATAL_IF_EQ(tmp, NULL, (stored_file_t*)
							HashTable_GetPointerToData(storage->files, (void*) victim_name));
				if (evicted)
				{
					RETURN_FATAL_IF_NEQ(err, 0, LinkedList_PushFront(tmp_evicted, victim_name,
							strlen(victim_name) + 1, tmp->contents, strlen(tmp->contents) + 1));
					fprintf(stderr, "[DEBUG %s:%d] SIZE: %lu\nCONTENTS:\n%s\n", __FILE__, __LINE__, tmp->contents_size, (char*)tmp->contents);
				}
				storage->storage_size -= tmp->contents_size;
				storage->files_no--;
				RETURN_FATAL_IF_NEQ(err, 0, HashTable_DeleteNode(storage->files, (void*) victim_name));
				free(victim_name);
			}
			if (evicted) *evicted = tmp_evicted;
			if (failure)
			{
				RETURN_FATAL_IF_NEQ(err, 0, RWLock_WriteUnlock(storage->lock));
				errno = EIDRM;
				return OP_FAILURE;
			}
		}
		new_contents = realloc(file->contents, file->contents_size + size);
		if (!new_contents) // realloc failed
		{
			RETURN_FATAL_IF_NEQ(err, 0, RWLock_WriteUnlock(storage->lock));
			errno = ENOMEM;
			return OP_FATAL;
		}
		file->contents = new_contents;
		memcpy(file->contents + file->contents_size, buf, size);
		file->contents_size += size;
		file->potential_writer = 0;
		storage->storage_size += size;
		RETURN_FATAL_IF_NEQ(err, 0, RWLock_WriteUnlock(storage->lock));
	}
	else // file is not inside the storage
	{
		RETURN_FATAL_IF_NEQ(err, 0, RWLock_WriteUnlock(storage->lock));
		errno = EBADF;
		return OP_FAILURE;
	}
	return OP_SUCCESS;
}

int
Storage_lockFile(storage_t* storage, const char* pathname, int client)
{
	if (!storage || !pathname)
	{
		errno = EINVAL;
		return OP_FAILURE;
	}

	int err, exists;
	stored_file_t* file;
	char str_client[BUFFERLEN];
	snprintf(str_client, BUFFERLEN, "%d", client);
	RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadLock(storage->lock));
	RETURN_FATAL_IF_EQ(exists, -1, HashTable_Find(storage->files, (void*) pathname));

	if (exists == 1) // file is inside the storage
	{
		RETURN_FATAL_IF_EQ(file, NULL, (stored_file_t*)
					HashTable_GetPointerToData(storage->files, (void*) pathname));

		RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadLock(file->rwlock));

		RETURN_FATAL_IF_EQ(err, -1, LinkedList_Contains(file->called_open, str_client));
		if (err == 1) // file has been opened by client
		{
			if (client == file->lock_owner) // client already owns the lock
			{
				RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadUnlock(file->rwlock));
				RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadUnlock(storage->lock));

				return OP_SUCCESS;
			}

			// to edit file struct params, lock
			// needs to be acquired in write mode.

			RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadUnlock(file->rwlock));

			RETURN_FATAL_IF_NEQ(err, 0, RWLock_WriteLock(file->rwlock));

			RETURN_FATAL_IF_NEQ(err, 0, pthread_mutex_lock(&(file->mutex_locked)));

			while (file->lock_owner != 0 && file->lock_owner != client)
				pthread_cond_wait(&(file->cond_locked), &(file->mutex_locked)); // cannot fail
			file->lock_owner = client;
			file->potential_writer = 0;

			RETURN_FATAL_IF_NEQ(err, 0, pthread_mutex_unlock(&(file->mutex_locked)));

			RETURN_FATAL_IF_NEQ(err, 0, RWLock_WriteUnlock(file->rwlock));
			RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadUnlock(storage->lock));
		}
		else // file has yet to be opened by client
		{
			RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadUnlock(file->rwlock));
			RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadUnlock(storage->lock));

			errno = EACCES;
			return OP_FAILURE;
		}
	}
	else // file is not inside the storage
	{
		RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadUnlock(storage->lock));
		errno = EBADF;
		return OP_FAILURE;
	}
	return OP_SUCCESS;
}

int
Storage_unlockFile(storage_t* storage, const char* pathname, int client)
{
	if (!storage || !pathname)
	{
		errno = EINVAL;
		return OP_FAILURE;
	}

	int err, exists;
	stored_file_t* file;
	char str_client[BUFFERLEN];
	snprintf(str_client, BUFFERLEN, "%d", client);
	RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadLock(storage->lock));
	RETURN_FATAL_IF_EQ(exists, -1, HashTable_Find(storage->files, (void*) pathname));
	if (exists == 1) // file is inside the storage
	{
		RETURN_FATAL_IF_EQ(file, NULL, (stored_file_t*)
					HashTable_GetPointerToData(storage->files, (void*) pathname));

		RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadLock(file->rwlock));

		RETURN_FATAL_IF_EQ(err, -1, LinkedList_Contains(file->called_open, str_client));
		if (err == 1) // file has been opened by client
		{
			if (client != file->lock_owner) // client does not own the lock.
			{
				RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadUnlock(file->rwlock));
				RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadUnlock(storage->lock));

				errno = EACCES;
				return OP_FAILURE;
			}

			// to edit file struct params, lock
			// needs to be acquired in write mode.
			RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadUnlock(file->rwlock));

			RETURN_FATAL_IF_NEQ(err, 0, RWLock_WriteLock(file->rwlock));

			RETURN_FATAL_IF_NEQ(err, 0, pthread_mutex_lock(&(file->mutex_locked)));
			file->lock_owner = 0;
			file->potential_writer = 0;
			pthread_cond_signal(&(file->cond_locked));
			RETURN_FATAL_IF_NEQ(err, 0, pthread_mutex_unlock(&(file->mutex_locked)));

			RETURN_FATAL_IF_NEQ(err, 0, RWLock_WriteUnlock(file->rwlock));
			RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadUnlock(storage->lock));

		}
		else // file has yet to be opened by client
		{
			RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadUnlock(file->rwlock));
			RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadUnlock(storage->lock));

			errno = EACCES;
			return OP_FAILURE;
		}

	}
	else // file is not inside the storage
	{
		RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadUnlock(storage->lock));
		errno = EBADF;
		return OP_FAILURE;
	}
	return OP_SUCCESS;
}

int
Storage_closeFile(storage_t* storage, const char* pathname, int client)
{
	if (!storage || !pathname)
	{
		errno = EINVAL;
		return OP_FAILURE;
	}

	int err, exists;
	stored_file_t* file;
	char str_client[BUFFERLEN];
	snprintf(str_client, BUFFERLEN, "%d", client);
	RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadLock(storage->lock));
	RETURN_FATAL_IF_EQ(exists, -1, HashTable_Find(storage->files, (void*) pathname));
	if (exists == 0) // file is not inside the storage
	{
		RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadUnlock(storage->lock));
		errno = EBADF;
		return OP_FAILURE;
	}
	else // file is inside the storage
	{
		// forcing it not to be const as it needs to be edited
		RETURN_FATAL_IF_EQ(file, NULL, (stored_file_t*)
					HashTable_GetPointerToData(storage->files, (void*) pathname));

		// file needs to be edited in write mode
		RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadLock(file->rwlock));

		// checking whether client has opened the file
		RETURN_FATAL_IF_EQ(err, -1, LinkedList_Contains(file->called_open, str_client));
		if (err == 0) // file has not been opened by client
		{
			RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadUnlock((file->rwlock)));
			RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadUnlock(storage->lock));
			errno = EACCES;
			return OP_FAILURE;
		}
		else // file has been opened: now closing it
		{
			RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadUnlock((file->rwlock)));
			RETURN_FATAL_IF_NEQ(err, 0, RWLock_WriteLock(file->rwlock));
			RETURN_FATAL_IF_NEQ(err, 0, LinkedList_Remove(file->called_open, str_client));
			file->potential_writer = 0;
			RETURN_FATAL_IF_NEQ(err, 0, RWLock_WriteUnlock(file->rwlock));
		}
	}
	RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadUnlock(storage->lock));
	return OP_SUCCESS;
}

int
Storage_removeFile(storage_t* storage, const char* pathname, int client)
{
	if (!storage || !pathname)
	{
		errno = EINVAL;
		return OP_FAILURE;
	}
	int err; // used as a placeholder for functions' return values
	int exists; // set to 1 if file is inside the storage
	stored_file_t* file; // used to denote file in storage corresponding pathname
	char str_client[BUFFERLEN]; // used when converting int client to a string

	snprintf(str_client, BUFFERLEN, "%d", client);
	RETURN_FATAL_IF_NEQ(err, 0, RWLock_WriteLock(storage->lock));
	RETURN_FATAL_IF_EQ(exists, -1, HashTable_Find(storage->files, (void*) pathname));

	fprintf(stderr, "[%s:%d] WL acquired over storage.\n", __FILE__, __LINE__);
	if (exists == 1) // file is inside storage
	{
		RETURN_FATAL_IF_EQ(file, NULL, (stored_file_t*)
					HashTable_GetPointerToData(storage->files, (void*) pathname));

		RETURN_FATAL_IF_EQ(err, -1, LinkedList_Contains(file->called_open, str_client));
		if (err == 0)
		{
			RETURN_FATAL_IF_NEQ(err, 0, RWLock_WriteUnlock(storage->lock));
			errno = EACCES;
			return OP_FAILURE;
		}
		if (file->lock_owner != client)
		{
			RETURN_FATAL_IF_NEQ(err, 0, RWLock_WriteUnlock(storage->lock));
			errno = EACCES;
			return OP_FAILURE;
		}
		fprintf(stderr, "[%s:%d] File is about to be deleted.\n", __FILE__, __LINE__);
		storage->storage_size -= file->contents_size;
		storage->files_no--;
		RETURN_FATAL_IF_EQ(err, -1, HashTable_DeleteNode(storage->files, (void*) pathname));
		RETURN_FATAL_IF_EQ(err, -1, LinkedList_Remove(storage->sorted_files, pathname));
		RETURN_FATAL_IF_NEQ(err, 0, RWLock_WriteUnlock(storage->lock));
	}
	else
	{
		RETURN_FATAL_IF_NEQ(err, 0, RWLock_WriteUnlock(storage->lock));
		errno = EBADF;
		return OP_FAILURE;
	}
	return OP_SUCCESS;
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
	printf("\tSorted files:\n");
	LinkedList_Print(storage->sorted_files);
	const node_t* curr;
	const stored_file_t* tmp;
	for (size_t i = 0; i < storage->max_files_no; i++)
	{
		printf("\n\tBucket no. %lu\n", i);
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
	RWLock_Free(storage->lock);
	LinkedList_Free(storage->sorted_files);
	HashTable_Free(storage->files);
	free(storage);
}