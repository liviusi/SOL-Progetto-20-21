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
#include <string.h>
#include <sys/stat.h>
#include <linux/limits.h>

static inline int
readn(long fd, void* buf, size_t size)
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

static inline int
writen(long fd, void* buf, size_t size)
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

static inline int
isNumber(const char* s, long* n) {
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

// source: https://gist.github.com/JonathonReinhart/8c0d90191c38af2dcadb102c4e202950
static inline int
mkdir_p(const char *path)
{
	const size_t len = strlen(path);
	char _path[PATH_MAX];
	char *p; 
	errno = 0;
	/* Copy string so its mutable */
	if (len > sizeof(_path)-1)
	{
		errno = ENAMETOOLONG;
		return -1; 
	}
	strcpy(_path, path);

	/* Iterate the string */
	for (p = _path + 1; *p; p++)
	{
		if (*p == '/')
		{
			/* Temporarily truncate */
			*p = '\0';

			if (mkdir(_path, S_IRWXU) != 0)
			{
				if (errno != EEXIST)
					return -1; 
			}

			*p = '/';
		}
	}

	if (mkdir(_path, S_IRWXU) != 0)
	{
		if (errno != EEXIST)
			return -1; 
	}

	return 0;
}

static inline int
savefile(const char* path, const char* contents)
{
	int len = strlen(path);
	char* tmp_path = (char*) malloc(sizeof(char) * (len + 1));
	if (!tmp_path) return -1;
	memset(tmp_path, 0, len + 1);
	strcpy(tmp_path, path);
	char* tmp = strrchr(tmp_path, '/');
	if (tmp) *tmp = '\0';
	if (mkdir_p(tmp_path) != 0)
	{
		fprintf(stderr, "[%s:%d] %s : reached this.\n", __FILE__, __LINE__, path);
		free(tmp_path);
		return -1;
	}
	mode_t mask = umask(033);
	FILE* file = fopen(path, "w+");
	if (!file)
	{
		fprintf(stderr, "[%s:%d] %s : reached this.\n", __FILE__, __LINE__, path);
		free(tmp_path);
		return -1;
	}
	umask(mask);
	if (contents)
	{
		if (fputs(contents, file) == EOF) return -1;
	}
	fclose(file);
	free(tmp_path);
	return 0;
}

#endif