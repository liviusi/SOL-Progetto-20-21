/**
 * @brief Source file for server_interface header.
 * @author Giacomo Trapani.
*/

#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdbool.h>
#include <stdio.h>
#include <server_defines.h>
#include <utilities.h>
#include <server_interface.h>
#include <wrappers.h>

#define MAXPATH 128
#define BUFFERLEN 256

#define HANDLE_ANSWER(buffer, bufferlen, answer, tmp, fatal_error) \
	READN_NUMBER((long) socket_fd, (void*) buffer, bufferlen, answer, err, failure); \
	switch (answer) \
	{ \
		case OP_FAILURE: \
			READN_NUMBER((long) socket_fd, (void*) buffer, bufferlen, tmp, err, failure); \
			err = (int) tmp; \
			goto failure; \
		case OP_FATAL: \
			READN_NUMBER((long) socket_fd, (void*) buffer, bufferlen, tmp, err, failure); \
			err = (int) tmp; \
			fatal_error = true; \
			break; \
		case OP_SUCCESS: \
			break; \
	}


static int socket_fd = -1;
static char socketpath[MAXPATH];
bool print_enabled = true;
bool exit_on_fatal_errors = true;

int
openConnection(const char* sockname, int msec, const struct timespec abstime)
{
	int err;
	char error_string[BUFFERLEN];
	if (socket_fd != -1)
	{
		err = EISCONN;
		goto failure;
	}
	if (!sockname || strlen(sockname) > MAXPATH || msec < 0)
	{
		err = EINVAL;
		goto failure;
	}

	socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (socket_fd != -1)
	{
		err = errno;
		goto failure;
	}

	struct sockaddr_un sock_addr;
	strncpy(sock_addr.sun_path, sockname, strlen(sockname) + 1);
	sock_addr.sun_family = AF_UNIX;

	time_t now;
	while(connect(socket_fd, (struct sockaddr*) &sock_addr, sizeof(sock_addr)) == -1)
	{
		err = errno;
		now = time(NULL);
		if (now >= abstime.tv_sec)
		{
			socket_fd = -1;
			err = EAGAIN;
			goto failure;
		}
		usleep(msec * 1000); // from milliseconds to microseconds
	}

	strncpy(socketpath, sockname, MAXPATH);

	PRINT_IF(print_enabled, "openConnection %s : SUCCESS.\n", sockname);
	return 0;

	failure:
		strerror_r(err, error_string, BUFFERLEN);
		PRINT_IF(print_enabled, "openConnection %s : FAILURE. errno = %s.\n", sockname,
					error_string);
		errno = err;
		return -1;
}

int
closeConnection(const char* sockname)
{
	int err;
	char error_string[BUFFERLEN];
	if (!sockname)
	{
		err = EINVAL;
		goto failure;
	}

	if (strcmp(sockname, socketpath) != 0)
	{
		err = ENOTCONN;
		goto failure;
	}

	if (close(socket_fd) == -1)
	{
		err = errno;
		socket_fd = -1;
		goto failure;
	}

	socket_fd = -1;

	PRINT_IF(print_enabled, "closeConnection %s : SUCCESS.\n", sockname);
	return 0;

	failure:
		strerror_r(err, error_string, BUFFERLEN);
		PRINT_IF(print_enabled, "closeConnection %s : FAILURE. errno = %s.\n", sockname,
					error_string);
		errno = err;
		return -1;
}

int
openFile(const char* pathname, int flags)
{
	int err;
	char error_string[BUFFERLEN];
	if (!pathname || strlen(pathname) > MAXPATH)
	{
		err = EINVAL;
		goto failure;
	}

	if (socket_fd == -1)
	{
		err = ENOTCONN;
		goto failure;
	}

	/**
	 * The actual open will be handled by the server;
	 * the client will send a buffer requesting it.
	 * The buffer will follow the following format:
	 * OPCODE(OPEN) PATHNAME FLAGS.
	*/

	char buffer[BUFFERLEN];
	int len = snprintf(buffer, BUFFERLEN, "%d %s %d", OPEN, pathname, flags);
	if (writen((long) socket_fd, (void*) buffer, len) == -1)
	{
		err = errno;
		goto failure;
	}

	memset(buffer, 0, BUFFERLEN);
	long answer, tmp;
	bool fatal_error = false;
	HANDLE_ANSWER(buffer, BUFFERLEN, answer, tmp, fatal_error);

	if (fatal_error) goto fatal;

	PRINT_IF(print_enabled, "openFile %s %d : SUCCESS.\n", pathname, flags);
	return 0;

	failure:
		strerror_r(err, error_string, BUFFERLEN);
		PRINT_IF(print_enabled, "openFile %s %d : FAILURE. errno = %s.\n", pathname, flags,
					error_string);
		errno = err;
		return -1;

	fatal:
		strerror_r(err, error_string, BUFFERLEN);
		PRINT_IF(print_enabled, "openFile %s %d : FATAL ERROR. errno = %s.\n", pathname,
					flags, error_string);
		errno = err;
		if (exit_on_fatal_errors) exit(errno);
		else return -1;
}


int
closeFile(const char* pathname)
{
	int err;
	char error_string[BUFFERLEN];
	if (!pathname || strlen(pathname) > MAXPATH)
	{
		err = EINVAL;
		goto failure;
	}

	if (socket_fd == -1)
	{
		err = ENOTCONN;
		goto failure;
	}

	/**
	 * The actual closure will be handled by the server;
	 * the client will send a buffer requesting it.
	 * The buffer will follow the following format:
	 * OPCODE(CLOSE) PATHNAME.
	*/

	char buffer[BUFFERLEN];
	int len = snprintf(buffer, BUFFERLEN, "%d %s", CLOSE, pathname);
	if (writen((long) socket_fd, (void*) buffer, len) == -1)
	{
		err = errno;
		goto failure;
	}

	memset(buffer, 0, BUFFERLEN);
	long answer, tmp;
	bool fatal_error = false;
	HANDLE_ANSWER(buffer, BUFFERLEN, answer, tmp, fatal_error);

	if (fatal_error) goto fatal;

	PRINT_IF(print_enabled, "closeFile %s : SUCCESS.\n", pathname);
	return 0;

	failure:
		strerror_r(err, error_string, BUFFERLEN);
		PRINT_IF(print_enabled, "closeFile %s : FAILURE. errno = %s.\n", pathname,
					error_string);
		errno = err;
		return -1;

	fatal:
		strerror_r(err, error_string, BUFFERLEN);
		PRINT_IF(print_enabled, "closeFile %s : FATAL ERROR. errno = %s.\n", pathname,
					error_string);
		errno = err;
		if (exit_on_fatal_errors) exit(errno);
		else return -1;
}

int
readFile(const char* pathname, void** buf, size_t* size)
{
	int err;
	char error_string[BUFFERLEN];
	if (!pathname || strlen(pathname) > MAXPATH)
	{
		err = EINVAL;
		goto failure;
	}

	if (socket_fd == -1)
	{
		err = ENOTCONN;
		goto failure;
	}
	if (buf) *buf = NULL;
	if (size) *size = 0;

	/**
	 * The actual reading will be handled by the server;
	 * the client will send a buffer requesting it.
	 * The buffer will follow the following format:
	 * OPCODE(READ) PATHNAME.
	*/

	char buffer[BUFFERLEN];
	int len;
	if (buf && size)
		len = snprintf(buffer, BUFFERLEN, "%d %s 1", READ, pathname);
	else
		len = snprintf(buffer, BUFFERLEN, "%d %s 0", READ, pathname);
	if (writen((long) socket_fd, (void*) buffer, len) == -1)
	{
		err = errno;
		goto failure;
	}

	memset(buffer, 0, BUFFERLEN);
	long answer, tmp;
	bool fatal_error = false;
	size_t tmp_size = 0;
	void* contents = NULL;
	// read result
	HANDLE_ANSWER(buffer, BUFFERLEN, answer, tmp, fatal_error);

	if (fatal_error) goto fatal;

	if (buf && size)
	{
		// read size
		READN_NUMBER((long) socket_fd, buffer, BUFFERLEN, answer, err, failure);
		tmp_size = (size_t) answer;
		contents = malloc(tmp_size);
		if (!contents)
		{
			err = ENOMEM;
			goto fatal;
		}
		memset(contents, 0, tmp_size);
		if (readn((long) socket_fd, (void*) contents, tmp_size) == -1)
		{
			err = errno;
			goto failure;
		}
	}

	*size = tmp_size;
	*buf = contents;
	return 0;

	failure:
		strerror_r(err, error_string, BUFFERLEN);
		PRINT_IF(print_enabled, "readFile %s : FAILURE. errno = %s.\n", pathname,
					error_string);
		errno = err;
		return -1;

	fatal:
		strerror_r(err, error_string, BUFFERLEN);
		PRINT_IF(print_enabled, "readFile %s : FATAL ERROR. errno = %s.\n", pathname,
					error_string);
		errno = err;
		if (exit_on_fatal_errors) exit(errno);
		else return -1;
}

int
writeFile(const char* pathname, const char* dirname)
{
	int err;
	char error_string[BUFFERLEN];
	if (!pathname || strlen(pathname) > MAXPATH)
	{
		err = EINVAL;
		goto failure;
	}

	if (socket_fd == -1)
	{
		err = ENOTCONN;
		goto failure;
	}

	/**
	 * The actual writing will be handled by the server;
	 * the client will send a buffer requesting it.
	 * The buffer will follow the following format:
	 * OPCODE(WRITE) PATHNAME DIRNAME.
	*/

	char buffer[BUFFERLEN];
	int len;
	if (dirname) 
		len = snprintf(buffer, BUFFERLEN, "%d %s %s", WRITE, pathname, dirname);
	else
		len = snprintf(buffer, BUFFERLEN, "%d %s", WRITE, pathname);
	if (writen((long) socket_fd, (void*) buffer, len) == -1)
	{
		err = errno;
		goto failure;
	}

	memset(buffer, 0, BUFFERLEN);
	long answer, tmp;
	bool fatal_error = false;
	char* contents = NULL;

	if (dirname)
	{
		READN_NUMBER((long) socket_fd, buffer, BUFFERLEN, answer, err, failure);
		switch (answer)
		{
			case FILE_NOT_EXPELLED:
				tmp = -1;
				break;
			case FILE_EXPELLED:
				// read number of files expelled
				READN_NUMBER((long) socket_fd, buffer, BUFFERLEN, tmp, err, failure);
				break;
		}
		while (tmp > 0)
		{
			// read content size
			READN_NUMBER((long) socket_fd, buffer, BUFFERLEN, answer, err, failure);
			contents = (char*) malloc(sizeof(char) * (answer + 1));
			GOTO_LABEL_IF_EQ(contents, NULL, err, fatal); // malloc failed
			if (readn((long) socket_fd, (void*) contents, (size_t) answer) == -1)
			{
				err = errno;
				goto failure;
			}
			contents[answer] = '\0';
			// should write contents
			free(contents);
			tmp--;
		}
	}

	HANDLE_ANSWER(buffer, BUFFERLEN, answer, tmp, fatal_error);

	if (fatal_error) goto fatal;

	if (dirname)
	{
		PRINT_IF(print_enabled, "writeFile %s %s : SUCCESS.\n", pathname, dirname);
	}
	else
	{
		PRINT_IF(print_enabled, "writeFile %s NULL : SUCCESS.\n", pathname);
	}
	return 0;

	failure:
		strerror_r(err, error_string, BUFFERLEN);
		if (dirname)
		{
			PRINT_IF(print_enabled, "writeFile %s %s : FAILURE. errno = %s.\n", pathname,
						dirname, error_string);
		}
		else
		{
			PRINT_IF(print_enabled, "writeFile %s NULL : FAILURE. errno = %s.\n", pathname,
						error_string);
		}
		errno = err;
		return -1;

	fatal:
		strerror_r(err, error_string, BUFFERLEN);
		if (dirname)
		{
			PRINT_IF(print_enabled, "writeFile %s %s : FATAL ERROR. errno = %s.\n", pathname,
						dirname, error_string);
		}
		else
		{
			PRINT_IF(print_enabled, "writeFile %s NULL : FATAL ERROR. errno = %s.\n", pathname,
						error_string);
		}
		errno = err;
		if (exit_on_fatal_errors) exit(errno);
		else return -1;
}

int
appendToFile(const char* pathname, void* buf, size_t size, const char* dirname)
{
	int err;
	char error_string[BUFFERLEN];
	if (!pathname || strlen(pathname) > MAXPATH)
	{
		err = EINVAL;
		goto failure;
	}

	if (socket_fd == -1)
	{
		err = ENOTCONN;
		goto failure;
	}

	/**
	 * The actual append will be handled by the server;
	 * the client will send a buffer requesting it.
	 * The buffer will follow the following format:
	 * OPCODE(APPEND) PATHNAME BUFFER SIZE DIRNAME.
	*/

	char buffer[BUFFERLEN];
	int len;
	if (dirname) 
		len = snprintf(buffer, BUFFERLEN, "%d %s %s %lu %s", APPEND, pathname, (char*) buf, size, dirname);
	else
		len = snprintf(buffer, BUFFERLEN, "%d %s %s %lu", APPEND, pathname, (char*) buf, size);
				
	if (writen((long) socket_fd, (void*) buffer, len) == -1)
	{
		err = errno;
		goto failure;
	}

	memset(buffer, 0, BUFFERLEN);
	long answer, tmp;
	bool fatal_error = false;
	char* contents = NULL;

	if (dirname)
	{
		READN_NUMBER((long) socket_fd, buffer, BUFFERLEN, answer, err, failure);
		switch (answer)
		{
			case FILE_NOT_EXPELLED:
				tmp = -1;
				break;
			case FILE_EXPELLED:
				// read number of files expelled
				READN_NUMBER((long) socket_fd, buffer, BUFFERLEN, tmp, err, failure);
				break;
		}
		while (tmp > 0)
		{
			// read content size
			READN_NUMBER((long) socket_fd, buffer, BUFFERLEN, answer, err, failure);
			contents = (char*) malloc(sizeof(char) * (answer + 1));
			GOTO_LABEL_IF_EQ(contents, NULL, err, fatal); // malloc failed
			if (readn((long) socket_fd, (void*) contents, (size_t) answer) == -1)
			{
				err = errno;
				goto failure;
			}
			contents[answer] = '\0';
			// should write contents
			free(contents);
			tmp--;
		}
	}

	HANDLE_ANSWER(buffer, BUFFERLEN, answer, tmp, fatal_error);

	if (fatal_error) goto fatal;

	PRINT_IF(print_enabled, "appendToFile %s %s : SUCCESS.\n", pathname, dirname);
	return 0;

	failure:
		strerror_r(err, error_string, BUFFERLEN);
		PRINT_IF(print_enabled, "appendToFile %s %s : FAILURE. errno = %s.\n", pathname,
					dirname, error_string);
		errno = err;
		return -1;

	fatal:
		strerror_r(err, error_string, BUFFERLEN);
		PRINT_IF(print_enabled, "appendToFile %s %s : FATAL ERROR. errno = %s.\n", pathname,
					dirname, error_string);
		errno = err;
		if (exit_on_fatal_errors) exit(errno);
		else return -1;
}