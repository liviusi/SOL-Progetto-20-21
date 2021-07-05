/**
 * @brief Header for utilities such as readn and writen.
 * @author Giacomo Trapani.
*/

#ifndef _UTILITIES_H_
#define _UTILITIES_H_

#define MBYTE 0.000001f

#include <errno.h>
#include <linux/limits.h> // PATH_MAX
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>


/**
 * @brief Reads up to given bytes from given descriptor, saves data to given pre-allocated buffer.
 * @returns read size on success, -1 on failure.
 * @exception The function may fail and set "errno" for any of the errors specified for the routine "read".
*/
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
		if (r == 0) return 0; // EOF
		left -= r;
		bufptr += r;
	}
	return size;
}

/**
 * @brief Writes buffer up to given size to given descriptor.
 * @returns 1 on success, -1 on failure.
 * @exception The function may fail and set "errno" for any of the errors specified for routine "write".
*/
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

/**
 * @brief Safely converts string to long.
 * @returns 0 on success, 2 on overflow/underflow, 1 otherwise.
 * @param s cannot be NULL, must contain at least a character.
 * @param n cannot be NULL.
 * @exception It sets "errno" to "EINVAL" if any param is not valid. The function may also fail and set "errno"
 * for any of the errors specified for the routine "strtol".
*/
static inline int
isNumber(const char* s, long* n)
{
	if (!n || !s || strlen(s) == 0)
	{
		errno = EINVAL; 
		return 1;
	}
	char* e = NULL;
	errno = 0;
	long val = strtol(s, &e, 10);
	if (errno == ERANGE) return 2; // overflow/underflow
	if (e != NULL && *e == (char)0)
	{
		*n = val;
		return 0;   // successo 
	}
	return 1;   // non e' un numero
}

/**
 * @brief Implementation of mkdir -p in C.
 * @returns 0 on success, -1 on failure.
 * @param path cannot be NULL or longer than system-defined PATH_MAX.
 * @exception It sets "errno" to "EINVAL" if any param is not valid. The function may also fail and set "errno"
 * for any of the errors specified in routine "mkdir".
 * @note source: https://gist.github.com/JonathonReinhart/8c0d90191c38af2dcadb102c4e202950
*/
static inline int
mkdir_p(const char *path)
{
	if (!path)
	{
		errno = EINVAL;
		return -1;
	}
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

/**
 * @brief Saves given content in given filename.
 * @returns 0 on success, -1 on failure.
 * @param path cannot be NULL.
 * @param contents cannot be NULL.
 * @exception It sets "errno" to "EINVAL" if any param is not valid. The function may also fail and set "errno"
 * for any of the errors specified for routines "malloc", "mkdir_p", "fopen", "fputs".
*/
static inline int
savefile(const char* path, const char* contents)
{
	if (!path || !contents)
	{
		errno = EINVAL;
		return -1;
	}
	int len = strlen(path);
	char* tmp_path = (char*) malloc(sizeof(char) * (len + 1));
	if (!tmp_path) return -1;
	memset(tmp_path, 0, len + 1);
	strcpy(tmp_path, path);
	char* tmp = strrchr(tmp_path, '/');
	if (tmp) *tmp = '\0';
	if (mkdir_p(tmp_path) != 0)
	{
		free(tmp_path);
		return -1;
	}
	mode_t mask = umask(033);
	FILE* file = fopen(path, "w+");
	if (!file)
	{
		free(tmp_path);
		return -1;
	}
	free(tmp_path);
	umask(mask);
	if (contents)
	{
		if (fputs(contents, file) == EOF)
		{
			fclose(file);
			return -1;
		}
	}
	fclose(file);
	return 0;
}

#endif