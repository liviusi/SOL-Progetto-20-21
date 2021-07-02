/**
 * @brief Source file for rwlock header.
 * @author Giacomo Trapani.
*/

#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>
#include <rwlock.h>
#include <wrappers.h>

struct _rwlock
{
	pthread_mutex_t mutex; // mutex is used to control access to struct
	pthread_cond_t cond;
	unsigned int readers; // number of readers
	bool pending_writer; // toggled on when there is a writer waiting
};

rwlock_t*
RWLock_Init()
{
	int err, errnocopy;
	rwlock_t* tmp = NULL;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	bool mutex_initialized = false, cond_initialized = false;

	err = pthread_mutex_init(&mutex, NULL);
	GOTO_LABEL_IF_NEQ(err, 0, errnocopy, failure);
	mutex_initialized = true;
	err = pthread_cond_init(&cond, NULL);
	GOTO_LABEL_IF_NEQ(err, 0, errnocopy, failure);
	cond_initialized = true;
	tmp = (rwlock_t*) malloc(sizeof(rwlock_t));
	GOTO_LABEL_IF_EQ(tmp, NULL, errnocopy, failure);

	tmp->cond = cond;
	tmp->mutex = mutex;
	tmp->pending_writer = false;
	tmp->readers = 0;
	return tmp;

	failure:
		if (mutex_initialized) pthread_mutex_destroy(&mutex);
		if (cond_initialized) pthread_cond_destroy(&cond);
		free(tmp);
		errno = errnocopy;
		return NULL;
}

int
RWLock_ReadLock(rwlock_t* lock)
{
	if (!lock)
	{
		errno = EINVAL;
		return -1;
	}
	int err;
	err = pthread_mutex_lock(&(lock->mutex));
	if (err != 0) return -1;
	while (lock->pending_writer) // priority goes to writing operations
		pthread_cond_wait(&(lock->cond), &(lock->mutex)); // there are no more writers
	lock->readers++; // increase reader count
	err = pthread_mutex_unlock(&(lock->mutex));
	if (err != 0) return -1;
	return 0;
}

int
RWLock_ReadUnlock(rwlock_t* lock)
{
	if (!lock)
	{
		errno = EINVAL;
		return -1;
	}
	int err;
	err = pthread_mutex_lock(&(lock->mutex));
	if (err != 0) return -1;
	lock->readers--;
	if (lock->readers == 0) // wake up writers as soon as there are no more readers
		pthread_cond_broadcast(&(lock->cond));
	err = pthread_mutex_unlock(&(lock->mutex));
	if (err != 0) return -1;
	return 0;
}

int
RWLock_WriteLock(rwlock_t* lock)
{
	if (!lock)
	{
		errno = EINVAL;
		return -1;
	}
	int err;
	err = pthread_mutex_lock(&(lock->mutex));
	if (err != 0) return -1;
	while (lock->pending_writer) // wait for other writers to be done
		pthread_cond_wait(&(lock->cond), &(lock->mutex));
	lock->pending_writer = true; // at this point no more readers are to be accepted
	while (lock->readers) // wait for readers to be done
		pthread_cond_wait(&(lock->cond), &(lock->mutex));
	err = pthread_mutex_unlock(&(lock->mutex));
	if (err != 0) return -1;
	return 0;
}

int
RWLock_WriteUnlock(rwlock_t* lock)
{
	if (!lock)
	{
		errno = EINVAL;
		return -1;
	}
	int err;
	err = pthread_mutex_lock(&(lock->mutex));
	if (err != 0) return -1;
	lock->pending_writer = false;
	pthread_cond_broadcast(&(lock->cond)); // there could be multiple readers waiting
	err = pthread_mutex_unlock(&(lock->mutex));
	if (err != 0) return -1;
	return 0;
}

void
RWLock_Free(rwlock_t* lock)
{
	if (!lock) return;
	pthread_mutex_destroy(&(lock->mutex));
	pthread_cond_destroy(&(lock->cond));
	free(lock);
}