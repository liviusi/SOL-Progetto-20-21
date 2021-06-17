/**
 * @brief Header file for custom made writer preferring read-write lock.
 * @author Giacomo Trapani.
*/

#ifndef _RWLOCK_H_
#define _RWLOCK_H_

// Struct fields are not exposed to maintain invariant.
typedef struct _rwlock rwlock_t;

/**
*/
rwlock_t*
RWLock_Init();

/**
*/
int
RWLock_ReadLock(rwlock_t*);

/**
*/
int
RWLock_ReadUnlock(rwlock_t*);

/**
*/
int
RWLock_WriteLock(rwlock_t*);

/**
*/
int
RWLock_WriteUnlock(rwlock_t*);

/**
*/
void
RWLock_Free(rwlock_t*);

#endif