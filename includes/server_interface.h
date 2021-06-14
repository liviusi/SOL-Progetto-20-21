/**
 * @brief Header file for the API to implement.
 * @author Giacomo Trapani.
*/

#define _DEFAULT_SOURCE

#ifndef _SERVER_INTERFACE_H_
#define _SERVER_INTERFACE_H_

int
openConnection(const char*, int, const struct timespec);

int
closeConnection(const char*);

int
openFile(const char*, int);

int
closeFile(const char*);

#endif