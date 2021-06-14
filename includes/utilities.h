/**
 * @brief Header for utilities such as readn and writen.
 * @author Giacomo Trapani.
*/

#ifndef _UTILITIES_H_
#define _UTILITIES_H_

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static inline int readn(long fd, void* buf, size_t size)
{
	size_t left = size;
	int r;
	char* bufptr = (char*) buf;
	while (left > 0)
	{
		if ((r = read((int) fd, bufptr, left)) == -1)
		{
			if (errno == EINTR) continue;
			return -1;
		}
		if (r == 0) return 0;   // EOF
		left -= r;
		bufptr += r;
	}
	return size;
}

static inline int writen(long fd, void* buf, size_t size)
{
	size_t left = size;
	int r;
	char* bufptr = (char*) buf;
	while (left > 0)
	{
		if ((r = write((int) fd, bufptr, left)) == -1)
		{
			if (errno == EINTR) continue;
			return -1;
		}
		if (r == 0) return 0;
		left -= r;
		bufptr += r;
	}
	return 1;
}

static inline int isNumber(const char* s, long* n) {
	if (s == NULL) return 1;
	if (strlen(s) == 0) return 1;
	char* e = NULL;
	errno = 0;
	long val = strtol(s, &e, 10);
	if (errno == ERANGE) return 2;    // overflow/underflow
	if (e != NULL && *e == (char)0)
	{
		*n = val;
		return 0;   // successo 
	}
	return 1;   // non e' un numero
}

#endif