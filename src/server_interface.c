/**
 * @brief Source file for server_interface header.
 * @author Giacomo Trapani.
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

#define ERRORSTRINGLEN 128
#define OPVALUE_LEN 2

#define GET_MESSAGE_SIZE(dest, pathname, buffer, buffer_size, dirname) \
	dest = 0; \
	dest += 2; \
	if (pathname) dest += strlen(pathname) + 1; \
	if (buffer) dest += buffer_size + 1; \
	dest += (size_t) snprintf(0, 0, "%lu", buffer_size) + 1;\
	if (dirname) dest += strlen(dirname);


static int fd_socket = -1;
static char socketpath[MAXPATH];
bool print_enabled = true;
bool exit_on_fatal_errors = true;

int
openConnection(const char* sockname, int msec, const struct timespec abstime)
{
	int err;
	char error_string[REQUESTLEN];
	if (fd_socket != -1)
	{
		err = EISCONN;
		goto failure;
	}
	if (!sockname || strlen(sockname) > MAXPATH || msec < 0)
	{
		err = EINVAL;
		goto failure;
	}

	fd_socket = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd_socket == -1)
	{
		err = errno;
		goto failure;
	}

	struct sockaddr_un sock_addr;
	strncpy(sock_addr.sun_path, sockname, strlen(sockname) + 1);
	sock_addr.sun_family = AF_UNIX;

	time_t now;
	while(connect(fd_socket, (struct sockaddr*) &sock_addr, sizeof(sock_addr)) == -1)
	{
		err = errno;
		now = time(NULL);
		if (now >= abstime.tv_sec)
		{
			fd_socket = -1;
			err = EAGAIN;
			goto failure;
		}
		usleep(msec * 1000); // from milliseconds to microseconds
	}

	strncpy(socketpath, sockname, MAXPATH);

	PRINT_IF(print_enabled, "openConnection %s : SUCCESS.\n", sockname);
	return 0;

	failure:
		strerror_r(err, error_string, REQUESTLEN);
		PRINT_IF(print_enabled, "openConnection %s : FAILURE. errno = %s.\n", sockname,
					error_string);
		errno = err;
		return -1;
}

int
closeConnection(const char* sockname)
{
	int err;
	char error_string[REQUESTLEN];
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

	if (close(fd_socket) == -1)
	{
		err = errno;
		fd_socket = -1;
		goto failure;
	}

	fd_socket = -1;

	PRINT_IF(print_enabled, "closeConnection %s : SUCCESS.\n", sockname);
	return 0;

	failure:
		strerror_r(err, error_string, REQUESTLEN);
		PRINT_IF(print_enabled, "closeConnection %s : FAILURE. errno = %s.\n", sockname,
					error_string);
		errno = err;
		return -1;
}

int
openFile(const char* pathname, int flags)
{
	int err;
	char error_string[ERRORSTRINGLEN];
	if (!pathname || strlen(pathname) > MAXPATH)
	{
		err = EINVAL;
		goto failure;
	}

	if (fd_socket == -1)
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

	char buffer[REQUESTLEN];
	memset(buffer, 0, REQUESTLEN);
	snprintf(buffer, REQUESTLEN, "%d %s %d", OPEN, pathname, flags);

	// it is necessary to send the whole buffer at this point
	if (writen((long) fd_socket, (void*) buffer, REQUESTLEN) == -1)
	{
		err = errno;
		goto failure;
	}
	// read actual output
	char answer_str[OPVALUE_LEN];
	memset(answer_str, 0, OPVALUE_LEN);
	if (readn((long) fd_socket, (void*) answer_str, OPVALUE_LEN) == -1)
	{
		err = errno;
		goto failure;
	}
	// check whether output is legal
	int answer;
	if (sscanf(answer_str, "%d", &answer) != 1)
	{
		err = EBADMSG;
		goto failure;
	}
	char errno_str[ERRNOLEN];
	// handle output
	switch (answer)
	{
		case OP_SUCCESS:
			break;

		case OP_FAILURE:
			// read errno value
			if (readn((long) fd_socket, (void*) errno_str, ERRNOLEN) == -1)
			{
				err = errno;
				goto failure;
			}
			if (sscanf(errno_str, "%d", &err) != 1)
			{
				err = EBADMSG;
				goto failure;
			}
			goto failure;

		case OP_FATAL:
			// read errno value
			if (readn((long) fd_socket, (void*) errno_str, ERRNOLEN) == -1)
			{
				err = errno;
				goto failure;
			}
			if (sscanf(errno_str, "%d", &err) != 1)
			{
				err = EBADMSG;
				goto failure;
			}
			goto fatal;
	}

	PRINT_IF(print_enabled, "openFile %s %d : SUCCESS.\n", pathname, flags);
	return 0;

	failure:
		strerror_r(err, error_string, REQUESTLEN);
		PRINT_IF(print_enabled, "openFile %s %d : FAILURE. errno = %s.\n", pathname, flags,
					error_string);
		errno = err;
		return -1;

	fatal:
		strerror_r(err, error_string, REQUESTLEN);
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
	char error_string[REQUESTLEN];
	if (!pathname || strlen(pathname) > MAXPATH)
	{
		err = EINVAL;
		goto failure;
	}

	if (fd_socket == -1)
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

	char buffer[REQUESTLEN];
	memset(buffer, 0, REQUESTLEN);
	if (buf && size)
		snprintf(buffer, REQUESTLEN, "%d %s 1", READ, pathname);
	else
		snprintf(buffer, REQUESTLEN, "%d %s 0", READ, pathname);

	// it is necessary to send the whole buffer at this point
	if (writen((long) fd_socket, (void*) buffer, REQUESTLEN) == -1)
	{
		err = errno;
		goto failure;
	}
	// read actual output
	char answer_str[OPVALUE_LEN];
	memset(answer_str, 0, OPVALUE_LEN);
	if (readn((long) fd_socket, (void*) answer_str, OPVALUE_LEN) == -1)
	{
		err = errno;
		goto failure;
	}
	// check whether output is legal
	int answer;
	if (sscanf(answer_str, "%d", &answer) != 1)
	{
		err = EBADMSG;
		goto failure;
	}
	char errno_str[ERRNOLEN];
	bool _failure = false, _fatal = false;
	// handle output
	switch (answer)
	{
		case OP_SUCCESS:
			break;

		case OP_FAILURE:
			// read errno value
			if (readn((long) fd_socket, (void*) errno_str, ERRNOLEN) == -1)
			{
				err = errno;
				goto failure;
			}
			if (sscanf(errno_str, "%d", &err) != 1)
			{
				err = EBADMSG;
				goto failure;
			}
			_failure = true;
			break;

		case OP_FATAL:
			// read errno value
			if (readn((long) fd_socket, (void*) errno_str, ERRNOLEN) == -1)
			{
				err = errno;
				goto failure;
			}
			if (sscanf(errno_str, "%d", &err) != 1)
			{
				err = EBADMSG;
				goto failure;
			}
			_fatal = true;
			break;
	}

	char* read_buffer = NULL;
	size_t read_size = 0;
	// read size
	char msg_size[SIZELEN];
	memset(msg_size, 0, SIZELEN);
	if (readn((long) fd_socket, (void*) msg_size, SIZELEN) == -1)
	{
		err = errno;
		goto failure;
	}
	if (sscanf(msg_size, "%lu", &read_size) != 1)
	{
		err = EBADMSG;
		goto failure;
	}
	// ensure there is enough space for the buffer
	// to be nul terminated
	if (read_size !=  0)
	{
		read_buffer = (char*) malloc(sizeof(char) * (read_size + 1));
		if (!read_buffer) // enomem
		{
			err = errno;
			goto fatal;
		}
		memset(read_buffer, 0, read_size + 1);
		if (readn((long) fd_socket, (void*) read_buffer, read_size) == -1)
		{
			err = errno;
			goto failure;
		}
		read_buffer[read_size] = '\0';
	}
	*size = read_size;
	// ensure buffer is null terminated
	*buf = (void*) read_buffer;

	if (_failure) goto failure;
	if (_fatal) goto fatal;

	PRINT_IF(print_enabled, "readFile %s : SUCCESS.\n", pathname);

	return 0;

	failure:
		strerror_r(err, error_string, REQUESTLEN);
		PRINT_IF(print_enabled, "readFile %s : FAILURE. errno = %s.\n", pathname,
					error_string);
		errno = err;
		return -1;

	fatal:
		strerror_r(err, error_string, REQUESTLEN);
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
	char error_string[REQUESTLEN];
	if (!pathname || strlen(pathname) > MAXPATH)
	{
		err = EINVAL;
		goto failure;
	}

	if (fd_socket == -1)
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

	char buffer[REQUESTLEN];
	memset(buffer, 0, REQUESTLEN);
	snprintf(buffer, REQUESTLEN, "%d %s", WRITE, pathname);

	// it is necessary to send the whole buffer at this point
	if (writen((long) fd_socket, (void*) buffer, REQUESTLEN) == -1)
	{
		err = errno;
		goto failure;
	}
	// read actual output
	char answer_str[OPVALUE_LEN];
	memset(answer_str, 0, OPVALUE_LEN);
	if (readn((long) fd_socket, (void*) answer_str, OPVALUE_LEN) == -1)
	{
		err = errno;
		goto failure;
	}
	// check whether output is legal
	int answer;
	if (sscanf(answer_str, "%d", &answer) != 1)
	{
		err = EBADMSG;
		goto failure;
	}
	char errno_str[ERRNOLEN];
	bool _failure = false, _fatal = false;
	// handle output
	switch (answer)
	{
		case OP_SUCCESS:
			break;

		case OP_FAILURE:
			// read errno value
			if (readn((long) fd_socket, (void*) errno_str, ERRNOLEN) == -1)
			{
				err = errno;
				goto failure;
			}
			if (sscanf(errno_str, "%d", &err) != 1)
			{
				err = EBADMSG;
				goto failure;
			}
			_failure = true;
			break;

		case OP_FATAL:
			// read errno value
			if (readn((long) fd_socket, (void*) errno_str, ERRNOLEN) == -1)
			{
				err = errno;
				goto failure;
			}
			if (sscanf(errno_str, "%d", &err) != 1)
			{
				err = EBADMSG;
				goto failure;
			}
			_fatal = true;
			break;
	}
	// handle evicted files
	// get number of victims
	char msg_size[SIZELEN];
	memset(msg_size, 0, SIZELEN);
	if (readn((long) fd_socket, (void*) msg_size, SIZELEN) == -1)
	{
		err = errno;
		goto failure;
	}
	size_t evicted_no = 0;
	if (sscanf(msg_size, "%lu", &evicted_no) != 1)
	{
		err = EBADMSG;
		goto failure;
	}
	if (evicted_no !=  0) // there have been victims
	{
		while (1)
		{
			if (evicted_no == 0) break;
			// get filename
			memset(buffer, 0, REQUESTLEN);
			if (readn((long) fd_socket, buffer, REQUESTLEN) == -1)
			{
				err = errno;
				goto failure;
			}
			// get content length
			memset(msg_size, 0, SIZELEN);
			if (readn((long) fd_socket, msg_size, SIZELEN) == -1)
			{
				err = errno;
				goto failure;
			}
			size_t content_size;
			if (sscanf(msg_size, "%lu", &content_size) != 1)
			{
				err = EBADMSG;
				goto failure;
			}
			char* contents = NULL;
			if (content_size != 0)
			{
				contents = (char*) malloc(content_size + 1);
				if (!contents)
				{
					err = errno;
					goto fatal;
				}
				memset(contents, 0, content_size + 1);
				if (readn((long) fd_socket, (void*) contents, content_size) == -1)
				{
					err = errno;
					goto failure;
				}
			}
			// files are to be stored if and only if dirname has been specified
			if (dirname)
			{
				// prepend dirname to filename
				size_t dir_len = strlen(dirname);
				if (dir_len + strlen(buffer) > PATH_MAX)
				{
					err = ENAMETOOLONG;
					goto failure;
				}
				memmove(buffer + dir_len, buffer, strlen(buffer) + 1);
				memcpy(buffer, dirname, dir_len);
				// save file
				if (savefile(buffer, contents) == -1)
				{
					err = errno;
					goto failure;
				}
			}
			free(contents); contents = NULL;
			evicted_no--;
		}
	}

	if (_failure) goto failure;
	if (_fatal) goto fatal;

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
		strerror_r(err, error_string, REQUESTLEN);
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
		strerror_r(err, error_string, REQUESTLEN);
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


int // IT IS YET TO BE REDONE.
appendToFile(const char* pathname, void* buf, size_t size, const char* dirname)
{
	int err;
	char error_string[REQUESTLEN];
	int len;
	GET_MESSAGE_SIZE(len, pathname, buf, size, dirname);
	if (!pathname || strlen(pathname) > MAXPATH ||
				len >= REQUESTLEN)
	{
		err = EINVAL;
		goto failure;
	}

	if (fd_socket == -1)
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

	char buffer[REQUESTLEN];
	memset(buffer, 0, REQUESTLEN);
	if (dirname) 
		snprintf(buffer, REQUESTLEN, "%d %s %s %lu %s", APPEND, pathname, (char*) buf, size, dirname);
	else
		snprintf(buffer, REQUESTLEN, "%d %s %s %lu", APPEND, pathname, (char*) buf, size);

	//HANDLER;

	// handle evicted files

	PRINT_IF(print_enabled, "appendToFile %s %s : SUCCESS.\n", pathname, dirname);
	return 0;

	failure:
		strerror_r(err, error_string, REQUESTLEN);
		PRINT_IF(print_enabled, "appendToFile %s %s : FAILURE. errno = %s.\n", pathname,
					dirname, error_string);
		errno = err;
		return -1;

	fatal:
		strerror_r(err, error_string, REQUESTLEN);
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
	char error_string[REQUESTLEN];
	if (!pathname || strlen(pathname) > MAXPATH)
	{
		err = EINVAL;
		goto failure;
	}

	if (fd_socket == -1)
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

	char buffer[REQUESTLEN];
	memset(buffer, 0, REQUESTLEN);
	snprintf(buffer, REQUESTLEN, "%d %s", LOCK, pathname);

	// it is necessary to send the whole buffer at this point
	if (writen((long) fd_socket, (void*) buffer, REQUESTLEN) == -1)
	{
		err = errno;
		goto failure;
	}
	// read actual output
	char answer_str[OPVALUE_LEN];
	memset(answer_str, 0, OPVALUE_LEN);
	if (readn((long) fd_socket, (void*) answer_str, OPVALUE_LEN) == -1)
	{
		err = errno;
		goto failure;
	}
	// check whether output is legal
	int answer;
	if (sscanf(answer_str, "%d", &answer) != 1)
	{
		err = EBADMSG;
		goto failure;
	}
	char errno_str[ERRNOLEN];
	// handle output
	switch (answer)
	{
		case OP_SUCCESS:
			break;

		case OP_FAILURE:
			// read errno value
			if (readn((long) fd_socket, (void*) errno_str, ERRNOLEN) == -1)
			{
				err = errno;
				goto failure;
			}
			if (sscanf(errno_str, "%d", &err) != 1)
			{
				err = EBADMSG;
				goto failure;
			}
			goto failure;

		case OP_FATAL:
			// read errno value
			if (readn((long) fd_socket, (void*) errno_str, ERRNOLEN) == -1)
			{
				err = errno;
				goto failure;
			}
			if (sscanf(errno_str, "%d", &err) != 1)
			{
				err = EBADMSG;
				goto failure;
			}
			goto fatal;
	}


	PRINT_IF(print_enabled, "lockFile %s : SUCCESS.\n", pathname);
	return 0;

	failure:
		strerror_r(err, error_string, REQUESTLEN);
		PRINT_IF(print_enabled, "lockFile %s : FAILURE. errno = %s.\n", pathname,
					error_string);
		errno = err;
		return -1;

	fatal:
		strerror_r(err, error_string, REQUESTLEN);
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
	char error_string[REQUESTLEN];
	if (!pathname || strlen(pathname) > MAXPATH)
	{
		err = EINVAL;
		goto failure;
	}

	if (fd_socket == -1)
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

	char buffer[REQUESTLEN];
	memset(buffer, 0, REQUESTLEN);
	snprintf(buffer, REQUESTLEN, "%d %s", UNLOCK, pathname);

	// it is necessary to send the whole buffer at this point
	if (writen((long) fd_socket, (void*) buffer, REQUESTLEN) == -1)
	{
		err = errno;
		goto failure;
	}
	// read actual output
	char answer_str[OPVALUE_LEN];
	memset(answer_str, 0, OPVALUE_LEN);
	if (readn((long) fd_socket, (void*) answer_str, OPVALUE_LEN) == -1)
	{
		err = errno;
		goto failure;
	}
	// check whether output is legal
	int answer;
	if (sscanf(answer_str, "%d", &answer) != 1)
	{
		err = EBADMSG;
		goto failure;
	}
	char errno_str[ERRNOLEN];
	// handle output
	switch (answer)
	{
		case OP_SUCCESS:
			break;

		case OP_FAILURE:
			// read errno value
			if (readn((long) fd_socket, (void*) errno_str, ERRNOLEN) == -1)
			{
				err = errno;
				goto failure;
			}
			if (sscanf(errno_str, "%d", &err) != 1)
			{
				err = EBADMSG;
				goto failure;
			}
			goto failure;

		case OP_FATAL:
			// read errno value
			if (readn((long) fd_socket, (void*) errno_str, ERRNOLEN) == -1)
			{
				err = errno;
				goto failure;
			}
			if (sscanf(errno_str, "%d", &err) != 1)
			{
				err = EBADMSG;
				goto failure;
			}
			goto fatal;
	}


	PRINT_IF(print_enabled, "unlockFile %s : SUCCESS.\n", pathname);
	return 0;

	failure:
		strerror_r(err, error_string, REQUESTLEN);
		PRINT_IF(print_enabled, "unlockFile %s : FAILURE. errno = %s.\n", pathname,
					error_string);
		errno = err;
		return -1;

	fatal:
		strerror_r(err, error_string, REQUESTLEN);
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
	char error_string[ERRORSTRINGLEN];
	if (!pathname || strlen(pathname) > MAXPATH)
	{
		err = EINVAL;
		goto failure;
	}

	if (fd_socket == -1)
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

	char buffer[REQUESTLEN];
	memset(buffer, 0, REQUESTLEN);
	snprintf(buffer, REQUESTLEN, "%d %s", CLOSE, pathname);

	// it is necessary to send the whole buffer at this point
	if (writen((long) fd_socket, (void*) buffer, REQUESTLEN) == -1)
	{
		err = errno;
		goto failure;
	}
	// read actual output
	char answer_str[OPVALUE_LEN];
	memset(answer_str, 0, OPVALUE_LEN);
	if (readn((long) fd_socket, (void*) answer_str, OPVALUE_LEN) == -1)
	{
		err = errno;
		goto failure;
	}
	// check whether output is legal
	int answer;
	if (sscanf(answer_str, "%d", &answer) != 1)
	{
		err = EBADMSG;
		goto failure;
	}
	char errno_str[ERRNOLEN];
	// handle output
	switch (answer)
	{
		case OP_SUCCESS:
			break;

		case OP_FAILURE:
			// read errno value
			if (readn((long) fd_socket, (void*) errno_str, ERRNOLEN) == -1)
			{
				err = errno;
				goto failure;
			}
			if (sscanf(errno_str, "%d", &err) != 1)
			{
				err = EBADMSG;
				goto failure;
			}
			goto failure;

		case OP_FATAL:
			// read errno value
			if (readn((long) fd_socket, (void*) errno_str, ERRNOLEN) == -1)
			{
				err = errno;
				goto failure;
			}
			if (sscanf(errno_str, "%d", &err) != 1)
			{
				err = EBADMSG;
				goto failure;
			}
			goto fatal;
	}

	PRINT_IF(print_enabled, "closeFile %s : SUCCESS.\n", pathname);
	return 0;

	failure:
		strerror_r(err, error_string, REQUESTLEN);
		PRINT_IF(print_enabled, "closeFile %s : FAILURE. errno = %s.\n", pathname,
					error_string);
		errno = err;
		return -1;

	fatal:
		strerror_r(err, error_string, REQUESTLEN);
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
	char error_string[REQUESTLEN];
	if (!pathname || strlen(pathname) > MAXPATH)
	{
		err = EINVAL;
		goto failure;
	}

	if (fd_socket == -1)
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

	char buffer[REQUESTLEN];
	memset(buffer, 0, REQUESTLEN);

	// it is necessary to send the whole buffer at this point
	if (writen((long) fd_socket, (void*) buffer, REQUESTLEN) == -1)
	{
		err = errno;
		goto failure;
	}
	// read actual output
	char answer_str[OPVALUE_LEN];
	memset(answer_str, 0, OPVALUE_LEN);
	if (readn((long) fd_socket, (void*) answer_str, OPVALUE_LEN) == -1) // !!deadlock here!!
	{
		err = errno;
		goto failure;
	}
	// check whether output is legal
	int answer;
	if (sscanf(answer_str, "%d", &answer) != 1)
	{
		err = EBADMSG;
		goto failure;
	}
	char errno_str[ERRNOLEN];
	// handle output
	switch (answer)
	{
		case OP_SUCCESS:
			break;

		case OP_FAILURE:
			// read errno value
			if (readn((long) fd_socket, (void*) errno_str, ERRNOLEN) == -1)
			{
				err = errno;
				goto failure;
			}
			if (sscanf(errno_str, "%d", &err) != 1)
			{
				err = EBADMSG;
				goto failure;
			}
			goto failure;

		case OP_FATAL:
			// read errno value
			if (readn((long) fd_socket, (void*) errno_str, ERRNOLEN) == -1)
			{
				err = errno;
				goto failure;
			}
			if (sscanf(errno_str, "%d", &err) != 1)
			{
				err = EBADMSG;
				goto failure;
			}
			goto fatal;
	}

	PRINT_IF(print_enabled, "removeFile %s : SUCCESS.\n", pathname);
	return 0;

	failure:
		strerror_r(err, error_string, REQUESTLEN);
		PRINT_IF(print_enabled, "removeFile %s : FAILURE. errno = %s.\n", pathname,
					error_string);
		errno = err;
		return -1;

	fatal:
		strerror_r(err, error_string, REQUESTLEN);
		PRINT_IF(print_enabled, "removeFile %s : FATAL ERROR. errno = %s.\n", pathname,
					error_string);
		errno = err;
		if (exit_on_fatal_errors) exit(errno);
		else return -1;
}