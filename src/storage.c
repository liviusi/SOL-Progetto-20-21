/**
 * @brief Source file for storage header. 
 * @author Giacomo Trapani.
*/

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pthread.h>
#include <unistd.h>

#include <hashtable.h>
#include <linked_list.h>
#include <server_defines.h>
#include <storage.h>
#include <rwlock.h>
#include <wrappers.h>

// Struct used to denote a file inside storage.
typedef struct _stored_file
{
	// actual data
	char* name; // file name
	void* contents; // file contents
	size_t contents_size; // size of file contents

	int lock_owner; // lock owner's fd; when there is none, it is set to 0.
	linked_list_t* called_open; // list of fds which called open on this file

	int potential_writer; // will be set to 0 if there is none
	rwlock_t* rwlock; // used for multithreading purposes

	// used for replacement algorithms
	time_t last_used;
	int frequency;
} stored_file_t;

/**
 * @brief Function to initialize file in storage.
 * @param name cannot be NULL.
 * @returns Initialized data structure on success, NULL on failure.
 * @exception It sets "errno" to "EINVAL" if any param is not valid. The function may fail and set "errno"
 * for any of the errors specified for the routines "malloc", "LinkedList_Init", "RWLock_Init".
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
	int errnocopy;

	tmp = (stored_file_t*) malloc(sizeof(stored_file_t));
	GOTO_LABEL_IF_EQ(tmp, NULL, errnocopy, init_failure);
	tmp_name = (char*) malloc(strlen(name) + 1);
	GOTO_LABEL_IF_EQ(tmp_name, NULL, errnocopy, init_failure);
	if (contents_size != 0 && contents)
	{
		tmp_contents = (void*) malloc(contents_size);
		GOTO_LABEL_IF_EQ(tmp_contents, NULL, errnocopy, init_failure);
	}
	tmp_called_open = LinkedList_Init(free);
	GOTO_LABEL_IF_EQ(tmp_called_open, NULL, errnocopy, init_failure);
	tmp_lock = RWLock_Init();
	GOTO_LABEL_IF_EQ(tmp_lock, NULL, errnocopy, init_failure);

	strncpy(tmp_name, name, strlen(name) + 1);
	memcpy(tmp_contents, contents, contents_size);
	tmp->name = tmp_name;
	tmp->contents = tmp_contents;
	tmp->contents_size = contents_size;
	tmp->lock_owner = 0;
	tmp->called_open = tmp_called_open;
	tmp->potential_writer = 0;
	tmp->rwlock = tmp_lock;
	tmp->frequency = 0;
	tmp->last_used = time(NULL);

	return tmp;

	init_failure:
		free(tmp_name);
		free(tmp_contents);
		LinkedList_Free(tmp_called_open);
		RWLock_Free(tmp_lock);
		free(tmp);
		errno = errnocopy;
		return NULL;
}

/**
 * Frees allocated resources.
*/
static void
StoredFile_Free(void* arg)
{
	if (!arg) return;
	stored_file_t* file = (stored_file_t*) arg;
	RWLock_Free(file->rwlock);
	LinkedList_Free(file->called_open);
	free(file->name);
	free(file->contents);
	free(file);
}

// Used when sorting files in storage according to LFU or LRU policies.
typedef struct usage
{
	char* name;
	time_t last_used;
	int frequency;
} usage_t;

// Comparison function used when calling "qsort" when storage's replacement algorithm has been set to LRU.
int
compare_usage_time(const void* a, const void* b)
{
	usage_t _a = *(usage_t*) a;
	usage_t _b = *(usage_t*) b;
	return difftime(_a.last_used, _b.last_used);
}

// Comparison function used when calling "qsort" when storage's replacement algorithm has been set to LFU.
int
compare_frequency(const void* a, const void* b)
{
	usage_t _a = *(usage_t*) a;
	usage_t _b = *(usage_t*) b;
	return (_a.frequency - _b.frequency);
}

struct _storage
{
	hashtable_t* files; // table of files in storage
	replacement_policy_t algorithm; // FIFO, LFU, LRU.
	linked_list_t* names; // list of file names

	size_t max_files_no; // maximum number of storeable files
	size_t max_storage_size; // maximum storage size
	size_t files_no; // current number of files
	size_t storage_size; // current storage size

	rwlock_t* lock; // used for multithreading purposes

	// as per requirements:
	size_t reached_files_no; // maximum reached number of files
	size_t reached_storage_size; // maximum reached storage size in bytes
	size_t evictions_no; // number of times replacement algorithm got triggered 
};

storage_t*
Storage_Init(size_t max_files_no, size_t max_storage_size, replacement_policy_t chosen_algo)
{
	if (max_files_no == 0 || max_storage_size == 0)
	{
		errno = EINVAL;
		return NULL;
	}
	int err;
	storage_t* tmp = NULL;
	linked_list_t* tmp_names = NULL;
	hashtable_t* tmp_files = NULL;
	rwlock_t* tmp_lock = NULL;
	tmp_lock = RWLock_Init();
	GOTO_LABEL_IF_EQ(tmp_lock, NULL, err, init_failure);
	tmp = (storage_t*) malloc(sizeof(storage_t));
	GOTO_LABEL_IF_EQ(tmp, NULL, err, init_failure);
	tmp_names = LinkedList_Init(free);
	GOTO_LABEL_IF_EQ(tmp_names, NULL, err, init_failure);
	tmp_files = HashTable_Init(max_files_no, NULL, NULL, StoredFile_Free);
	GOTO_LABEL_IF_EQ(tmp_files, NULL, err, init_failure);

	tmp->algorithm = chosen_algo;
	tmp->files = tmp_files;
	tmp->names = tmp_names;
	tmp->lock = tmp_lock;
	tmp->max_files_no = max_files_no;
	tmp->max_storage_size = max_storage_size;
	tmp->storage_size = 0;
	tmp->files_no = 0;
	tmp->reached_files_no = 0;
	tmp->reached_storage_size = 0;
	tmp->evictions_no = 0;

	return tmp;

	init_failure:
		err = errno;
		RWLock_Free(tmp_lock);
		LinkedList_Free(tmp_names);
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
 * @exception It sets "errno" to "EINVAL" if any param is not valid. The function may also fail and set "errno"
 * for any of the errors specified for the routines "LinkedList_PopBack", "LinkedList_PopFront", "LinkedList_CopyAllKeys". 
*/
static int
Storage_getVictim(storage_t* storage, char** victim_name)
{
	if (!victim_name || !storage)
	{
		errno = EINVAL;
		return -1;
	}
	int errnocopy;
	size_t i = 0, j = 0;
	usage_t* stats = NULL;
	linked_list_t* copy = NULL;
	const stored_file_t* tmp = NULL;
	char* name = NULL;
	char* victim = NULL;
	switch (storage->algorithm)
	{
		case FIFO:
			errno = 0;
			i = LinkedList_PopBack(storage->names, victim_name, NULL);
			if (i == 0 && errno == ENOMEM) return -1;
			return 0;

		case LFU:
			// sort files by usage frequency
			stats = (usage_t*) malloc(sizeof(usage_t) * storage->files_no);
			if (!stats) goto failure;
			copy = LinkedList_CopyAllKeys(storage->names);
			if (!copy) goto failure;
			while (LinkedList_GetNumberOfElements(copy) != 0)
			{
				errno = 0;
				if (LinkedList_PopFront(copy, &name, NULL) == 0 && errno == ENOMEM) goto failure;
				stats[i].name = (char*) malloc(sizeof(char) * (strlen(name) + 1));
				if (!stats[i].name) goto failure;
				tmp = (const stored_file_t*) HashTable_GetPointerToData(storage->files, name);
				if (!tmp) goto failure;
				strcpy(stats[i].name, name);
				stats[i].last_used = tmp->last_used;
				stats[i].frequency = tmp->frequency;
				i++;
				free(name);
			}
			qsort((void*) stats, i, sizeof(usage_t), compare_frequency);
			victim = (char*) malloc(sizeof(char) * (strlen(stats[0].name) + 1));
			if (!victim) goto failure;
			strcpy(victim, stats[0].name);
			// remove victim from names
			LinkedList_Remove(storage->names, victim);
			*victim_name = victim;
			while (j != i) { free(stats[j].name); j++; }
			free(stats);
			LinkedList_Free(copy);
			return 0;

		case LRU:
			// sort files by last usage time
			stats = (usage_t*) malloc(sizeof(usage_t) * storage->files_no);
			if (!stats) goto failure;
			copy = LinkedList_CopyAllKeys(storage->names);
			if (!copy) goto failure;
			while (LinkedList_GetNumberOfElements(copy) != 0)
			{
				errno = 0;
				if (LinkedList_PopFront(copy, &name, NULL) == 0 && errno == ENOMEM) goto failure;
				stats[i].name = (char*) malloc(sizeof(char) * (strlen(name) + 1));
				if (!stats[i].name) goto failure;
				tmp = (const stored_file_t*) HashTable_GetPointerToData(storage->files, name);
				if (!tmp) goto failure;
				strcpy(stats[i].name, name);
				stats[i].last_used = tmp->last_used;
				stats[i].frequency = tmp->frequency;
				i++;
				free(name);
			}
			qsort((void*) stats, i, sizeof(usage_t), compare_usage_time);
			victim = (char*) malloc(sizeof(char) * (strlen(stats[0].name) + 1));
			if (!victim) goto failure;
			strcpy(victim, stats[0].name);
			// remove victim from names
			LinkedList_Remove(storage->names, victim);
			*victim_name = victim;
			while (j != i) { free(stats[j].name); j++; }
			free(stats);
			LinkedList_Free(copy);
			return 0;
	}
	return -1;

	failure:
		errnocopy = errno;
		j = 0;
		while (j != i) { free(stats[j].name); j++; }
		free(stats);
		LinkedList_Free(copy);
		free(name);
		errno = errnocopy;
		return -1;
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
	char str_client[SIZELEN]; // used to denote client as a string

	int len = snprintf(str_client, SIZELEN, "%d", client); // converting client to a string
	bool w_lock = IS_O_CREATE_SET(flags);

	/**
	 * If there is an attempt to create a new file, lock over storage needs to be acquired in writer mode as
	 * the number of files inside storage may be increased.
	*/

	// acquire lock over storage
	if (!w_lock) { RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadLock(storage->lock)); }
	else { RETURN_FATAL_IF_NEQ(err, 0, RWLock_WriteLock(storage->lock)); }

	// check whether file exists
	RETURN_FATAL_IF_EQ(exists, -1, HashTable_Find(storage->files, (void*) pathname));

	if (exists == 1 && w_lock) // file exists, creation flag is set
	{
		RETURN_FATAL_IF_NEQ(err, 0, RWLock_WriteUnlock(storage->lock));
		errno = EEXIST;
		return OP_FAILURE;
	}
	if (exists == 0) // file is not inside the storage
	{
		if (!w_lock) // creation flag is not set
		{
			RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadUnlock(storage->lock));
			errno = ENOENT;
			return OP_FAILURE;
		}
		if (storage->files_no == storage->max_files_no)
		{
			RETURN_FATAL_IF_NEQ(err, 0, RWLock_WriteUnlock(storage->lock));
			errno = ENOSPC;
			return OP_FAILURE;
		}
		else // add file to storage
		{
			storage->files_no++;
			RETURN_FATAL_IF_EQ(file, NULL, StoredFile_Init(pathname, NULL, 0));
			if (IS_O_LOCK_SET(flags))
				file->lock_owner = client; // client owns this file's lock
			if (IS_O_LOCK_SET(flags) && w_lock)
				file->potential_writer = client; // client can write this file
			RETURN_FATAL_IF_NEQ(err, 0, LinkedList_PushFront(file->called_open, str_client, len+1, NULL, 0));
			RETURN_FATAL_IF_EQ(err, -1, HashTable_Insert(storage->files, (void*) pathname, strlen(pathname) + 1,
						(void*) file, sizeof(*file)));
			RETURN_FATAL_IF_EQ(err, -1, LinkedList_PushFront(storage->names, pathname, strlen(pathname) + 1, NULL, 0));
			// file has been copied inside storage
			free(file);
		}
	}
	else // file is already inside the storage
	{
		// forcing it not to be const as it needs to be edited
		RETURN_FATAL_IF_EQ(file, NULL, (stored_file_t*) HashTable_GetPointerToData(storage->files, (void*) pathname));
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
			RETURN_FATAL_IF_NEQ(err, 0 , LinkedList_PushFront(file->called_open, str_client, len + 1, NULL, 0));
			// edit file usage params
			file->last_used = time(NULL);
			file->frequency++;
			RETURN_FATAL_IF_NEQ(err, 0, RWLock_WriteUnlock(file->rwlock));

		}

	}
	// release lock over storage
	if (!w_lock) { RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadUnlock(storage->lock)); }
	else { RETURN_FATAL_IF_NEQ(err, 0, RWLock_WriteUnlock(storage->lock)); }
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
	char str_client[SIZELEN]; // used to denote client as a string
	stored_file_t* file;
	void* tmp_contents = NULL; // used to denote file contents
	size_t tmp_size = 0; // used to denote file size

	*buf = NULL; *size = 0; // initializing both params to improve readability
	snprintf(str_client, SIZELEN, "%d", client); // convert int client to string
	RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadLock(storage->lock));

	RETURN_FATAL_IF_EQ(exists, -1, HashTable_Find(storage->files, (void*) pathname));

	if (exists == 1) // file is inside the storage
	{
		RETURN_FATAL_IF_EQ(file, NULL, (stored_file_t*) HashTable_GetPointerToData(storage->files, (void*) pathname));
		RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadLock(file->rwlock));
		// a client already owns this file's lock
		if (file->lock_owner != 0 && file->lock_owner != client)
		{
			RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadUnlock(file->rwlock));
			RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadUnlock(storage->lock));;
			errno = EPERM;
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
				// edit file usage params
				file->last_used = time(NULL);
				file->frequency++;
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

int
Storage_readNFiles(storage_t* storage, linked_list_t** read_files, size_t n, int client)
{
	if (!storage || !read_files)
	{
		errno = EINVAL;
		return OP_FAILURE;
	}

	int err;
	char str_client[SIZELEN]; // used to denote client as a string
	stored_file_t* file = NULL;
	linked_list_t* names = NULL; // list of file names in storage
	linked_list_t* tmp = NULL;

	snprintf(str_client, SIZELEN, "%d", client); // convert int client to string
	RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadLock(storage->lock));

	if (storage->files_no == 0) // storage is empty
	{
		*read_files = NULL;
		RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadUnlock(storage->lock));
		return OP_SUCCESS;
	}
	// storage is not empty
	RETURN_FATAL_IF_EQ(names, NULL, LinkedList_CopyAllKeys(storage->names));
	int readfiles_no = 0;
	int attempts = 0;
	bool read_all = ((n == 0) || (n >= storage->files_no));
	RETURN_FATAL_IF_EQ(tmp, NULL, LinkedList_Init(NULL));
	while (1)
	{
		// if n files have been read and n is not 0
		// or
		// every file in storage has been read (successfully or not)
		if ((readfiles_no == n && !read_all) || (read_all && attempts == storage->files_no)) break;
		char* pathname = NULL;
		RETURN_FATAL_IF_EQ(err, -1, LinkedList_PopFront(names, &pathname, NULL));
		RETURN_FATAL_IF_EQ(file, NULL, (stored_file_t*) HashTable_GetPointerToData(storage->files, (void*) pathname));
		RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadLock(file->rwlock));
		// a client already owns this file's lock
		if (file->lock_owner != 0 && file->lock_owner != client)
		{
			RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadUnlock(file->rwlock));
			free(pathname);
			attempts++;
			continue;
		}
		/**
		 * readNFiles shall work on files yet to be opened by client as doing this any other way would kill its purpose.
		*/
		// file is empty
		if (file->contents_size == 0 || !file->contents)
		{
			RETURN_FATAL_IF_NEQ(err, 0, LinkedList_PushBack(tmp, pathname, strlen(pathname) + 1, NULL, 0));
			RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadUnlock(file->rwlock));
			RETURN_FATAL_IF_NEQ(err, 0, RWLock_WriteLock(file->rwlock));
			file->potential_writer = 0;
			// edit file usage params
			file->last_used = time(NULL);
			file->frequency++;
			RETURN_FATAL_IF_NEQ(err, 0, RWLock_WriteUnlock(file->rwlock));
			free(pathname);
			readfiles_no++;
			attempts++;
			continue;
		}
		else
		{
			RETURN_FATAL_IF_NEQ(err, 0, LinkedList_PushBack(tmp, pathname, strlen(pathname) + 1, file->contents,
						file->contents_size + 1));
			RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadUnlock(file->rwlock));
			RETURN_FATAL_IF_NEQ(err, 0, RWLock_WriteLock(file->rwlock));
			file->potential_writer = 0;
			// edit file usage params
			file->last_used = time(NULL);
			file->frequency++;
			RETURN_FATAL_IF_NEQ(err, 0, RWLock_WriteUnlock(file->rwlock));
			free(pathname);
			readfiles_no++;
			attempts++;
			continue;
		}
	}
	LinkedList_Free(names);
	*read_files = tmp;
	RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadUnlock(storage->lock));
	return OP_SUCCESS;
}

int
Storage_writeFile(storage_t* storage, const char* pathname, size_t length, const char* contents,
			linked_list_t** evicted, int client)
{
	if (!storage || !pathname)
	{
		errno = EINVAL;
		return OP_FAILURE;
	}

	int err, exists;
	bool failure = false; // toggled on if replacement algorithm chooses pathname as a victim
	char* copy_contents = NULL; // buffer of contents
	char* victim_name = NULL; // used to denote victim's name
	stored_file_t* stored_file = NULL; // used to denote pathname as a file inside storage
	stored_file_t* tmp = NULL; // used to denote victim as a file inside storage
	linked_list_t* tmp_evicted = NULL; // list of evicted files

	// file must not be bigger than storage's size
	if (length > storage->max_storage_size)
	{
		if (evicted) *evicted = tmp_evicted;
		errno = EFBIG;
		return OP_FAILURE;
	}

	// file is not empty
	if (length != 0)
	{
		// +1 for '\0'
		RETURN_FATAL_IF_EQ(copy_contents, NULL, (char*) malloc(sizeof(char) * (length + 1)));
		memcpy(copy_contents, contents, length);
		copy_contents[length] = '\0'; // string needs to be null terminated
	}

	// as files may be deleted, no readers are allowed on storage
	RETURN_FATAL_IF_NEQ(err, 0, RWLock_WriteLock(storage->lock));

	// update storage info
	storage->reached_files_no = MAX(storage->reached_files_no, storage->files_no);
	storage->reached_storage_size = MAX(storage->reached_storage_size, storage->storage_size);

	RETURN_FATAL_IF_EQ(exists, -1, HashTable_Find(storage->files, (void*) pathname));

	if (exists == 1) // file is inside the storage
	{
		// as there can be at most one writer at a time, no locks need to be acquired over file
		RETURN_FATAL_IF_EQ(stored_file, NULL, (stored_file_t*) HashTable_GetPointerToData(storage->files, (void*) pathname));

		if (stored_file->potential_writer != client) // client cannot write this file
		{
			if (evicted) *evicted = tmp_evicted;
			free(copy_contents);
			RETURN_FATAL_IF_NEQ(err, 0, RWLock_WriteUnlock(storage->lock));
			errno = EACCES;
			return OP_FAILURE;
		}
		// there's no room for this file
		// start replacement algorithm
		if (storage->storage_size + length > storage->max_storage_size)
		{
			storage->evictions_no++;
			if (evicted) // if a list is specified, save evicted files' data
				{ RETURN_FATAL_IF_EQ(tmp_evicted, NULL, LinkedList_Init(NULL)); }
			while (!failure)
			{
				if (storage->storage_size + length <= storage->max_storage_size) break;
				RETURN_FATAL_IF_NEQ(err, 0, Storage_getVictim(storage, &victim_name));
				if (strcmp(victim_name, pathname) == 0) // file to be written got evicted
					failure = true;
				RETURN_FATAL_IF_EQ(tmp, NULL, (stored_file_t*) HashTable_GetPointerToData(storage->files, (void*) victim_name));
				if (evicted)
				{
					RETURN_FATAL_IF_NEQ(err, 0, LinkedList_PushFront(tmp_evicted, victim_name,
								strlen(victim_name) + 1, tmp->contents, tmp->contents_size));
				}
				storage->storage_size -= tmp->contents_size; // update storage size
				storage->files_no--; // update number of files
				RETURN_FATAL_IF_NEQ(err, 0, HashTable_DeleteNode(storage->files, (void*) victim_name));
				free(victim_name);
			}
			if (evicted) *evicted = tmp_evicted;
			if (failure) // file to be written got evicted
			{
				free(copy_contents);
				RETURN_FATAL_IF_NEQ(err, 0, RWLock_WriteUnlock(storage->lock));
				errno = EIDRM;
				return OP_FAILURE;
			}
		}
		if (copy_contents) // file is not empty
		{
			stored_file->contents_size = length;
			stored_file->contents = (void*) copy_contents;
		}
		stored_file->potential_writer = 0;
		storage->storage_size += length;
		RETURN_FATAL_IF_NEQ(err, 0, RWLock_WriteUnlock(storage->lock));

	}
	else
	{
		free(copy_contents);
		RETURN_FATAL_IF_NEQ(err, 0, RWLock_WriteUnlock(storage->lock));
		errno = EBADF;
		return OP_FAILURE;
	}
	return OP_SUCCESS;
}

int
Storage_appendToFile(storage_t* storage, const char* pathname, void* buf, size_t size, linked_list_t** evicted, int client)
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
	char str_client[SIZELEN]; // used when converting int client to a string

	snprintf(str_client, SIZELEN, "%d", client);
	RETURN_FATAL_IF_NEQ(err, 0, RWLock_WriteLock(storage->lock));

	// update storage info
	storage->reached_files_no = MAX(storage->reached_files_no, storage->files_no);
	storage->reached_storage_size = MAX(storage->reached_storage_size, storage->storage_size);

	RETURN_FATAL_IF_EQ(exists, -1, HashTable_Find(storage->files, (void*) pathname));

	if (exists == 1) // file is inside the storage
	{
		RETURN_FATAL_IF_EQ(file, NULL, (stored_file_t*) HashTable_GetPointerToData(storage->files, (void*) pathname));
		RETURN_FATAL_IF_EQ(err, -1, LinkedList_Contains(file->called_open, str_client));
		if (err == 0) // file has not been opened by this client
		{
			RETURN_FATAL_IF_NEQ(err, 0, RWLock_WriteUnlock(storage->lock));
			errno = EACCES;
			return OP_FAILURE;
		}
		if (file->lock_owner != client && file->lock_owner != 0) // if the lock is held by a different client
		{
			RETURN_FATAL_IF_NEQ(err, 0, RWLock_WriteUnlock(storage->lock));
			errno = EPERM;
			return OP_FAILURE;
		}
		if (size == 0 || buf == NULL) // there is nothing to append
		{
			RETURN_FATAL_IF_NEQ(err, 0, RWLock_WriteUnlock(storage->lock));
			return OP_SUCCESS;
		}
		if (storage->storage_size + size > storage->max_storage_size) // handle eventual file deletion
		{
			storage->evictions_no++;
			if (evicted) RETURN_FATAL_IF_EQ(tmp_evicted, NULL, LinkedList_Init(NULL));
			while (!failure)
			{
				if (storage->storage_size + size <= storage->max_storage_size) break;
				RETURN_FATAL_IF_NEQ(err, 0, Storage_getVictim(storage, &victim_name));
				if (strcmp(victim_name, pathname) == 0) failure = true;
				RETURN_FATAL_IF_EQ(tmp, NULL, (stored_file_t*)
							HashTable_GetPointerToData(storage->files, (void*) victim_name));
				if (evicted)
				{
					RETURN_FATAL_IF_NEQ(err, 0, LinkedList_PushFront(tmp_evicted, victim_name,
								strlen(victim_name) + 1, tmp->contents, strlen(tmp->contents) + 1));
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
	char str_client[SIZELEN];

	snprintf(str_client, SIZELEN, "%d", client);
	RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadLock(storage->lock));
	RETURN_FATAL_IF_EQ(exists, -1, HashTable_Find(storage->files, (void*) pathname));

	if (exists == 1) // file is inside the storage
	{
		RETURN_FATAL_IF_EQ(file, NULL, (stored_file_t*) HashTable_GetPointerToData(storage->files, (void*) pathname));
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
			// to edit file struct params, lock needs to be acquired in write mode.
			RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadUnlock(file->rwlock));
			RETURN_FATAL_IF_NEQ(err, 0, RWLock_WriteLock(file->rwlock));
			if (file->lock_owner != 0 && file->lock_owner != client) // some client already owns lock
			{
				RETURN_FATAL_IF_NEQ(err, 0, RWLock_WriteUnlock(file->rwlock));
				RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadUnlock(storage->lock));
				errno = EPERM;
				return OP_FAILURE;
			}
			file->lock_owner = client;
			file->potential_writer = 0;
			// edit file usage params
			file->last_used = time(NULL);
			file->frequency++;

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
	char str_client[SIZELEN];

	snprintf(str_client, SIZELEN, "%d", client);
	RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadLock(storage->lock));
	RETURN_FATAL_IF_EQ(exists, -1, HashTable_Find(storage->files, (void*) pathname));

	if (exists == 1) // file is inside the storage
	{
		RETURN_FATAL_IF_EQ(file, NULL, (stored_file_t*) HashTable_GetPointerToData(storage->files, (void*) pathname));

		RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadLock(file->rwlock));

		RETURN_FATAL_IF_EQ(err, -1, LinkedList_Contains(file->called_open, str_client));
		if (err == 1) // file has been opened by client
		{
			if (client != file->lock_owner) // client does not own the lock.
			{
				RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadUnlock(file->rwlock));
				RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadUnlock(storage->lock));
				errno = EPERM;
				return OP_FAILURE;
			}

			// to edit file struct params, lock needs to be acquired in write mode.
			RETURN_FATAL_IF_NEQ(err, 0, RWLock_ReadUnlock(file->rwlock));
			RETURN_FATAL_IF_NEQ(err, 0, RWLock_WriteLock(file->rwlock));
			file->lock_owner = 0;
			file->potential_writer = 0;
			// edit file usage params
			file->last_used = time(NULL);
			file->frequency++;

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
	char str_client[SIZELEN];

	snprintf(str_client, SIZELEN, "%d", client);
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
		RETURN_FATAL_IF_EQ(file, NULL, (stored_file_t*) HashTable_GetPointerToData(storage->files, (void*) pathname));

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
			// edit file usage params
			file->last_used = time(NULL);
			file->frequency++;
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
	char str_client[SIZELEN]; // used when converting int client to a string

	snprintf(str_client, SIZELEN, "%d", client);
	RETURN_FATAL_IF_NEQ(err, 0, RWLock_WriteLock(storage->lock));

	// update storage info
	storage->reached_files_no = MAX(storage->reached_files_no, storage->files_no);
	storage->reached_storage_size = MAX(storage->reached_storage_size, storage->storage_size);

	RETURN_FATAL_IF_EQ(exists, -1, HashTable_Find(storage->files, (void*) pathname));

	if (exists == 1) // file is inside storage
	{
		RETURN_FATAL_IF_EQ(file, NULL, (stored_file_t*) HashTable_GetPointerToData(storage->files, (void*) pathname));

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
			errno = EPERM;
			return OP_FAILURE;
		}
		storage->storage_size -= file->contents_size;
		storage->files_no--;
		RETURN_FATAL_IF_EQ(err, -1, HashTable_DeleteNode(storage->files, (void*) pathname));
		RETURN_FATAL_IF_EQ(err, -1, LinkedList_Remove(storage->names, pathname));
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

size_t
Storage_GetReachedFiles(storage_t* storage)
{
	if (!storage)
	{
		errno = EINVAL;
		return 0;
	}
	size_t res = 0;
	if (RWLock_WriteLock(storage->lock) != 0) return 0;
	storage->reached_files_no = MAX(storage->files_no, storage->reached_files_no);
	res = storage->reached_files_no;
	if (RWLock_WriteUnlock(storage->lock) != 0) return 0;
	return res;
}

size_t
Storage_GetReachedSize(storage_t* storage)
{
	if (!storage)
	{
		errno = EINVAL;
		return 0;
	}
	size_t res = 0;
	if (RWLock_WriteLock(storage->lock) != 0) return 0;
	storage->reached_storage_size = MAX(storage->reached_storage_size, storage->storage_size);
	res = storage->reached_storage_size;
	if (RWLock_WriteUnlock(storage->lock) != 0) return 0;
	return res;
}

void
Storage_Print(storage_t* storage)
{
	// update storage info
	storage->reached_files_no = MAX(storage->reached_files_no, storage->files_no);
	storage->reached_storage_size = MAX(storage->reached_storage_size, storage->storage_size);
	printf("\nSTORAGE DETAILS\n");
	printf("MAXIMUM AMOUNT OF FILES STORED:\t%lu.\n", storage->reached_files_no);
	printf("MAXIMUM STORAGE SIZE REACHED:\t%5f / %5f [MB].\n", storage->reached_storage_size * MBYTE, storage->max_storage_size * MBYTE);
	printf("REPLACEMENT ALGORITHM GOT TRIGGERED:\t%lu times.\n", storage->evictions_no);
	printf("STORAGE CONTAINS:\t");
	LinkedList_Print(storage->names);
}

void
Storage_Free(storage_t* storage)
{
	if (!storage) return;
	RWLock_Free(storage->lock);
	LinkedList_Free(storage->names);
	HashTable_Free(storage->files);
	free(storage);
}