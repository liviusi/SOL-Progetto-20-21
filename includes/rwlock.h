/**
 * @brief Header file for custom made writer preferring read-write lock.
 * @author Giacomo Trapani.
*/

#ifndef _RWLOCK_H_
#define _RWLOCK_H_

#ifdef DEBUG
#include <pthread.h>
#include <stdbool.h>
struct _rwlock
{
	pthread_mutex_t mutex; // mutex is used to control access to struct
	pthread_cond_t cond;
	unsigned int readers; // number of readers
	bool pending_writer; // toggled on when there is a writer waiting
};
#endif

// Struct fields are not exposed to maintain invariant.
typedef struct _rwlock rwlock_t;

/**
 * @brief Initializes read write lock.
 * @returns Pointer to initialized lock on success, NULL on failure.
 * @exception It sets errno to ENOMEM if and only if needed memory allocation fails.
*/
rwlock_t*
RWLock_Init();

/**
 * @brief Locks for reading.
 * @returns 0 on success, -1 on failure.
 * @exception It sets errno to EINVAL if lock is NULL.
*/
int
RWLock_ReadLock(rwlock_t* lock);

/**
 * @brief Unlocks for reading.
 * @returns 0 on success, -1 on failure.
 * @exception It sets errno to EINVAL if lock is NULL.
*/
int
RWLock_ReadUnlock(rwlock_t* lock);

/**
 * @brief Locks for writing.
 * @returns 0 on success, -1 on failure.
 * @exception It sets errno to EINVAL if lock is NULL.
*/
int
RWLock_WriteLock(rwlock_t* lock);

/**
 * @brief Unlocks for writing.
 * @returns 0 on success, -1 on failure.
 * @exception It sets errno to EINVAL if lock is NULL.
*/
int
RWLock_WriteUnlock(rwlock_t* lock);

/**
 * Frees allocated resources.
*/
void
RWLock_Free(rwlock_t* lock);

#endif