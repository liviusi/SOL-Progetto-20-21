/**
 * @brief Source file for server_interface header.
 * @author Giacomo Trapani.
 * IT IS TO BE REDONE.
*/

#define _DEFAULT_SOURCE

#include <errno.h>
#include <math.h>
#include <linux/limits.h>
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
#include <server_interface.h>
#include <utilities.h>
#include <wrappers.h>

#define BUFFERLEN 2048
#define ANSWERLEN 2
#define CHUNK 100

#define GET_MESSAGE_SIZE(dest, pathname, buffer, buffer_size, dirname) \
	dest = 0; \
	dest += 2; \
	if (pathname) dest += strlen(pathname) + 1; \
	if (buffer) dest += buffer_size + 1; \
	dest += (size_t) snprintf(0, 0, "%lu", buffer_size) + 1;\
	if (dirname) dest += strlen(dirname);


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
	if (socket_fd == -1)
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
	memset(buffer, 0, BUFFERLEN);
	snprintf(buffer, BUFFERLEN, "%d %s %d", OPEN, pathname, flags);

	//HANDLER;

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
	memset(buffer, 0, BUFFERLEN);
	if (buf && size)
		snprintf(buffer, BUFFERLEN, "%d %s 1", READ, pathname);
	else
		snprintf(buffer, BUFFERLEN, "%d %s 0", READ, pathname);

	//HANDLER;

	void* read_buffer = NULL;
	size_t read_size = 0;

	memset(buffer, 0, BUFFERLEN);
	if (readn((long) socket_fd, buffer, BUFFERLEN) == -1)
	{
		err = errno;
		goto failure;
	}
	if (sscanf(buffer, "%lu", &read_size) != 1)
	{
		err = EBADMSG;
		goto failure;
	}
	// ensure there is enough space for the buffer
	// to be nul terminated
	read_buffer = malloc(read_size + 1);
	if (!read_buffer) // enomem
	{
		err = errno;
		goto fatal;
	}
	memset(read_buffer, 0, read_size + 1);
	if (readn((long) socket_fd, read_buffer, read_size) == -1)
	{
		err = errno;
		goto failure;
	}
	*size = read_size;
	*buf = read_buffer;

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
	char msg_size[MSG_SIZELEN];
	bool _failure = false;
	bool _fatal = false;
	memset(buffer, 0, BUFFERLEN);
	snprintf(buffer, BUFFERLEN, "%d %s", WRITE, pathname);

	fprintf(stderr, "[%d] sending %s\n", __LINE__, buffer);
	buffer[BUFFERLEN-1] = '\0';
	if (writen((long) socket_fd, (void*) buffer, strlen(buffer) + 1) == -1)
	{
		err = errno;
		goto failure;
	}
	memset(buffer, 0, BUFFERLEN);
	int answer;
	
	// handle evicted files
	// prepend dirname to name
	//size_t dir_len = strlen(dirname);
	//if (dir_len + strlen(name) > PATH_MAX)
	//{
	//	err = ENAMETOOLONG;
	//	goto failure;
	//}
	//memmove(name + dir_len, name, strlen(name) + 1);
	//memcpy(name, dirname, dir_len);

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
	int len;
	GET_MESSAGE_SIZE(len, pathname, buf, size, dirname);
	if (!pathname || strlen(pathname) > MAXPATH ||
				len >= BUFFERLEN)
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
	memset(buffer, 0, BUFFERLEN);
	if (dirname) 
		snprintf(buffer, BUFFERLEN, "%d %s %s %lu %s", APPEND, pathname, (char*) buf, size, dirname);
	else
		snprintf(buffer, BUFFERLEN, "%d %s %s %lu", APPEND, pathname, (char*) buf, size);

	//HANDLER;

	// handle evicted files

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

int
lockFile(const char* pathname)
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
	 * The actual locking will be handled by the server;
	 * the client will send a buffer requesting it.
	 * The buffer will follow the following format:
	 * OPCODE(LOCK) PATHNAME.
	*/

	char buffer[BUFFERLEN];
	memset(buffer, 0, BUFFERLEN);
	snprintf(buffer, BUFFERLEN, "%d %s", LOCK, pathname);

	//HANDLER;

	PRINT_IF(print_enabled, "lockFile %s : SUCCESS.\n", pathname);
	return 0;

	failure:
		strerror_r(err, error_string, BUFFERLEN);
		PRINT_IF(print_enabled, "lockFile %s : FAILURE. errno = %s.\n", pathname,
					error_string);
		errno = err;
		return -1;

	fatal:
		strerror_r(err, error_string, BUFFERLEN);
		PRINT_IF(print_enabled, "lockFile %s : FATAL ERROR. errno = %s.\n", pathname,
					error_string);
		errno = err;
		if (exit_on_fatal_errors) exit(errno);
		else return -1;
}

int
unlockFile(const char* pathname)
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
	 * The actual unlocking will be handled by the server;
	 * the client will send a buffer requesting it.
	 * The buffer will follow the following format:
	 * OPCODE(LOCK) PATHNAME.
	*/

	char buffer[BUFFERLEN];
	memset(buffer, 0, BUFFERLEN);
	snprintf(buffer, BUFFERLEN, "%d %s", UNLOCK, pathname);

	//HANDLER;

	PRINT_IF(print_enabled, "unlockFile %s : SUCCESS.\n", pathname);
	return 0;

	failure:
		strerror_r(err, error_string, BUFFERLEN);
		PRINT_IF(print_enabled, "unlockFile %s : FAILURE. errno = %s.\n", pathname,
					error_string);
		errno = err;
		return -1;

	fatal:
		strerror_r(err, error_string, BUFFERLEN);
		PRINT_IF(print_enabled, "unlockFile %s : FATAL ERROR. errno = %s.\n", pathname,
					error_string);
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
	memset(buffer, 0, BUFFERLEN);
	snprintf(buffer, BUFFERLEN, "%d %s", CLOSE, pathname);

	//HANDLER;

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
removeFile(const char* pathname)
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
	 * The actual removal will be handled by the server;
	 * the client will send a buffer requesting it.
	 * The buffer will follow the following format:
	 * OPCODE(CLOSE) PATHNAME.
	*/

	char buffer[BUFFERLEN];
	memset(buffer, 0, BUFFERLEN);

	//HANDLER;

	PRINT_IF(print_enabled, "removeFile %s : SUCCESS.\n", pathname);
	return 0;

	failure:
		strerror_r(err, error_string, BUFFERLEN);
		PRINT_IF(print_enabled, "removeFile %s : FAILURE. errno = %s.\n", pathname,
					error_string);
		errno = err;
		return -1;

	fatal:
		strerror_r(err, error_string, BUFFERLEN);
		PRINT_IF(print_enabled, "removeFile %s : FATAL ERROR. errno = %s.\n", pathname,
					error_string);
		errno = err;
		if (exit_on_fatal_errors) exit(errno);
		else return -1;
}