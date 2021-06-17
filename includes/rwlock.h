/**
 * @brief Header file for custom made writer preferring read-write lock.
 * @author Giacomo Trapani.
*/

#ifndef _RWLOCK_H_
#define _RWLOCK_H_

#ifdef DEBUG
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
*/
rwlock_t*
RWLock_Init();

/**
*/
int
RWLock_ReadLock(rwlock_t* lock);

/**
*/
int
RWLock_ReadUnlock(rwlock_t* lock);

/**
*/
int
RWLock_WriteLock(rwlock_t* lock);

/**
*/
int
RWLock_WriteUnlock(rwlock_t* lock);

/**
*/
void
RWLock_Free(rwlock_t* lock);

#endif