/**
 * @brief Header file for custom made writer preferring read-write lock.
 * @author Giacomo Trapani.
*/

#ifndef _RWLOCK_H_
#define _RWLOCK_H_

// Struct fields are not exposed to force callee to access it using the implemented methods.
typedef struct _rwlock rwlock_t;

/**
 * @brief Initializes read write lock.
 * @returns Pointer to initialized lock on success, NULL on failure.
 * @exception It sets "errno" for any of the errors specified for the routines "malloc",
 * "pthread_mutex_init", "pthread_cond_init".
*/
rwlock_t*
RWLock_Init();

/**
 * @brief Locks for reading.
 * @returns 0 on success, -1 on failure.
 * @param lock cannot be NULL.
 * @exception It sets "errno" to "EINVAL" if any param is not valid. The function may also fail and set "errno"
 * for any of the errors specified for the routines "pthread_mutex_lock", "pthread_mutex_unlock".
*/
int
RWLock_ReadLock(rwlock_t* lock);

/**
 * @brief Unlocks for reading.
 * @returns 0 on success, -1 on failure.
 * @param lock cannot be NULL.
 * @exception It sets "errno" to "EINVAL" if any param is not valid. The function may also fail and set "errno"
 * for any of the errors specified for the routines "pthread_mutex_lock", "pthread_mutex_unlock".
*/
int
RWLock_ReadUnlock(rwlock_t* lock);

/**
 * @brief Locks for writing.
 * @returns 0 on success, -1 on failure.
 * @param lock cannot be NULL.
 * @exception It sets "errno" to "EINVAL" if any param is not valid. The function may also fail and set "errno"
 * for any of the errors specified for the routines "pthread_mutex_lock", "pthread_mutex_unlock".
*/
int
RWLock_WriteLock(rwlock_t* lock);

/**
 * @brief Unlocks for writing.
 * @returns 0 on success, -1 on failure.
 * @param lock cannot be NULL.
 * @exception It sets "errno" to "EINVAL" if any param is not valid. The function may also fail and set "errno"
 * for any of the errors specified for the routines "pthread_mutex_lock", "pthread_mutex_unlock".
*/
int
RWLock_WriteUnlock(rwlock_t* lock);

/**
 * Frees allocated resources.
*/
void
RWLock_Free(rwlock_t* lock);

#endif