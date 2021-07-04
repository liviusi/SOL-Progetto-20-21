/**
 * @brief Defines used both by server and client.
 * @author Giacomo Trapani.
*/

#ifndef _OPCODES_H_
#define _OPCODES_H_

#define SET_FLAG(mask, flag) mask |= flag // sets flag in mask
#define RESET_MASK(mask) mask = 0 // resets mask

#define O_CREATE 1 // flag used when creating new files
#define O_LOCK 2 // flag used when locking a file
#define IS_O_CREATE_SET(mask) (mask & 1) // evaluated to true if O_CREATE has been set
#define IS_O_LOCK_SET(mask) ((mask >> 1) & 1) // evaluated to true if O_LOCK has been set

#define OP_SUCCESS 0 // file system operations' return value on success
#define OP_FAILURE 1 // file system operations' return value on failure
#define OP_FATAL 2 // file system operations' return value on fatal errors

#define ERRNOLEN 4 // maximum characters needed to write errno value as a string
#define MAXPATH 108
#define SIZELEN 32 // used when converting numerical types to strings
#define REQUESTLEN 2048

// Used to denote allowed operations on file system
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

// Used to denote implemented replacement policies
typedef enum _replacement_policy
{
	FIFO,
	LRU,
	LFU
} replacement_policy_t;

#endif