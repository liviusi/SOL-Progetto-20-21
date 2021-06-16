/**
 * @brief Header file for the filesystem.
 * @author Giacomo Trapani.
*/

#ifndef _STORAGE__H_
#define _STORAGE_H_

#include <stdlib.h>
typedef struct _storage storage_t;

typedef enum _replacement_algo
{
	FIFO
} replacement_algo_t;

storage_t*
Storage_Init(size_t, size_t, replacement_algo_t);

void
Storage_Print(const storage_t*);

void
Storage_Free(storage_t*);

int
Storage_openFile(storage_t*, const char*, int, int);

#endif