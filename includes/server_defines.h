/**
 * @brief Defines used both by server and client.
 * @author Giacomo Trapani.
*/

#ifndef _OPCODES_H_
#define _OPCODES_H_

#define SET_FLAG(mask, flag) mask |= flag
#define RESET_MASK(mask) mask = 0

#define O_CREATE 1
#define O_LOCK 2
#define IS_O_CREATE_SET(mask) mask & 1
#define IS_O_LOCK_SET(mask) ((mask >> 1) & 1)

#define OP_SUCCESS 0
#define OP_FAILURE 1
#define OP_FATAL 2

#define ERRNOLEN 4
#define MAXPATH 108
#define MAXFILESIZE 1000000 // 1 MB
#define SIZELEN 32
#define REQUESTLEN 2048

typedef enum opcodes
{
	OPEN,
	CLOSE,
	READ,
	WRITE,
	APPEND,
	READ_N,
	LOCK,
	UNLOCK,
	REMOVE,
	TERMINATE
} opcodes_t;

#endif