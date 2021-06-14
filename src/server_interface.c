/**
 * @brief Source fille for server_interface header.
 * @author Giacomo Trapani.
*/

#define _DEFAULT_SOURCE

#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdbool.h>
#include <stdio.h>
#include <server_defines.h>
#include <utilities.h>
#include <server_interface.h>

#define MAXPATH 108
#define BUFFERLEN 256

#define HANDLE_ANSWER(buffer, bufferlen, operation, argument) \
	if (readn((long) socket_fd, (void*) buffer, bufferlen) == -1) \
		return -1; \
	long answer; \
	if (isNumber(buffer, &answer) != 0) \
	{ \
		errno = EINVAL; \
		return -1; \
	} \
	if (answer != OP_SUCCESS) \
	{ \
		errno = EBADE; \
		return -1; \
	}

#define RESET_LAST_OP \
	last_op.open = false; \
	last_op.lock = false; \
	last_op.create = false; \
	memset(last_op.pathname, 0, MAXPATH);

typedef struct _last_operation
{
	bool open;
	bool lock;
	bool create;
	char pathname[MAXPATH];
} last_operation_t;


static int socket_fd = -1;
static char socketpath[MAXPATH];
static last_operation_t last_op = { .open = false, .lock = false, .create = false };

int
openConnection(const char* sockname, int msec, const struct timespec abstime)
{
	if (socket_fd != -1)
	{
		errno = EISCONN;
		return -1;
	}
	if (!sockname || strlen(sockname) > MAXPATH || msec < 0)
	{
		errno = EINVAL;
		return -1;
	}

	socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (socket_fd != -1) return -1;

	struct sockaddr_un sock_addr;
	strncpy(sock_addr.sun_path, sockname, strlen(sockname) + 1);
	sock_addr.sun_family = AF_UNIX;

	int err;
	time_t now;
	while(connect(socket_fd, (struct sockaddr*) &sock_addr, sizeof(sock_addr)) == -1)
	{
		err = errno;
		now = time(NULL);
		if (now >= abstime.tv_sec)
		{
			socket_fd = -1;
			errno = EAGAIN;
			return -1;
		}
		usleep(msec * 1000); // from milliseconds to microseconds
	}

	strncpy(socketpath, sockname, MAXPATH);
	return 0;
}

int
closeConnection(const char* sockname)
{
	if (!sockname)
	{
		errno = EINVAL;
		return -1;
	}

	if (strcmp(sockname, socketpath) != 0)
	{
		errno = ENOTCONN;
		return -1;
	}

	if (close(socket_fd) == -1)
	{
		socket_fd = -1;
		return -1;
	}

	socket_fd = -1;
	return 0;
}

int
openFile(const char* pathname, int flags)
{
	if (!pathname || strlen(pathname) > MAXPATH)
	{
		errno = EINVAL;
		return -1;
	}

	if (socket_fd == -1)
	{
		errno = ENOTCONN;
		return -1;
	}

	/**
	 * The actual open will be handled by the server;
	 * the client will send a buffer requesting it.
	 * The buffer will follow the following format:
	 * OPCODE(OPEN) PATHNAME FLAGS.
	*/

	char buffer[BUFFERLEN];
	int len = snprintf(buffer, BUFFERLEN, "%d %s %d", OPEN, pathname, flags);
	if (writen((long) socket_fd, (void*) buffer, len) == -1) return -1;

	memset(buffer, 0, BUFFERLEN);
	HANDLE_ANSWER(buffer, BUFFERLEN, OPEN, pathname);

	last_op.open = true;
	last_op.lock = IS_O_LOCK_SET(flags);
	last_op.create = IS_O_CREATE_SET(flags);
	strncpy(last_op.pathname, pathname, MAXPATH);

	return 0;
}


int
closeFile(const char* pathname)
{
	if (!pathname || strlen(pathname) > MAXPATH)
	{
		errno = EINVAL;
		return -1;
	}

	if (socket_fd == -1)
	{
		errno = ENOTCONN;
		return -1;
	}

	/**
	 * The actual closure will be handled by the server;
	 * the client will send a buffer requesting it.
	 * The buffer will follow the following format:
	 * OPCODE(CLOSE) PATHNAME FLAGS.
	*/

	char buffer[BUFFERLEN];
	int len = snprintf(buffer, BUFFERLEN, "%d %s", CLOSE, pathname);
	if (writen((long) socket_fd, (void*) buffer, len) == -1) return -1;

	memset(buffer, 0, BUFFERLEN);
	HANDLE_ANSWER(buffer, BUFFERLEN, CLOSE, pathname);

	RESET_LAST_OP;

	return 0;
}
