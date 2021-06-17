#include <rwlock.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>

int x = 0;
rwlock_t* lock;
bool ok = true;

static void* writer(void* arg)
{
	while (x < 20)
	{
		RWLock_WriteLock(lock);
		fprintf(stderr, "Writer has acquired lock\n");
		x++;
		RWLock_WriteUnlock(lock);
		fprintf(stderr, "Writer has released lock\n");
		sleep(1);
	}
	return (void*) 0;
}

static void* reader(void* arg)
{
	while (ok)
	{
		RWLock_ReadLock(lock);
		fprintf(stderr, "Reader has acquired lock\n");
		fprintf(stdout, "x = %d\n", x);
		fflush(stdout);
		RWLock_ReadUnlock(lock);
		fprintf(stderr, "Reader has released lock\n");
	}
	return (void*) 0;
}

int main()
{
	pthread_t tid1, tid2, tid3, tid4;
	rwlock_t* tmp = RWLock_Init();
	lock = tmp;
	pthread_create(&tid1, NULL, &writer, NULL);
	pthread_create(&tid2, NULL, &reader, NULL);
	sleep(10);
	pthread_create(&tid3, NULL, &writer, NULL);
	pthread_create(&tid4, NULL, &reader, NULL);
	sleep(10);
	ok = false;
	pthread_join(tid1, NULL);
	pthread_join(tid2, NULL);
	pthread_join(tid3, NULL);
	pthread_join(tid4, NULL);
	RWLock_Free(tmp);
	return 0;
}