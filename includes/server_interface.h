/**
 * @brief Header file for the API to implement.
 * @author Giacomo Trapani.
*/

#ifndef _SERVER_INTERFACE_H_
#define _SERVER_INTERFACE_H_

#include <sys/time.h>

extern bool print_enabled;
extern bool exit_on_fatal_errors;

/**
*/
int
openConnection(const char* sockname, int msec, const struct timespec abstime);

/**
*/
int
closeConnection(const char* sockname);

/**
*/
int
openFile(const char* pathname, int flags);

/**
*/
int
closeFile(const char* pathname);

/**
*/
int
readFile(const char* pathname, void** buf, size_t* size);

/**
*/
int
writeFile(const char* pathname, const char* dirname);

#endif