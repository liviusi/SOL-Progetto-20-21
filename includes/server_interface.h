/**
 * @brief Header file for the API to implement.
 * @author Giacomo Trapani.
*/

#ifndef _SERVER_INTERFACE_H_
#define _SERVER_INTERFACE_H_

#include <sys/time.h>

extern bool print_enabled; // toggled on when client enables output on stdout
extern bool exit_on_fatal_errors; // if toggled on, when any fatal error occurs in the filesystem the client exits.

/**
 * @brief Connects client to given socket name making an attempt every given msec till given absolute time.
 * @returns 0 on success, -1 on failure.
 * @param sockname cannot be NULL, its length must be less than 108.
 * @param msec cannot be negative.
 * @exception It sets "errno" to "EINVAL" if any param is not valid, to "EISCONN" if the client is already connected
 * to a socket, to "EAGAIN" if the client could not succeed before given absolute time. The function may also fail and set
 * "errno" for any of the errors specified for the routines "connect", "socket".
*/
int
openConnection(const char* sockname, int msec, const struct timespec abstime);

/**
 * @brief Closes connect from client to given socket name.
 * @returns 0 on success, -1 on failure.
 * @param sockname cannot be NULL.
 * @exception It sets "errno" to "EINVAL" if any param is not valid, to "ENOTCONN" if client is not connected to given socket.
 * The function may also fail and set "errno" for any of the errors specified for the routines "close", "writen".
*/
int
closeConnection(const char* sockname);

/**
 * @brief Requests server to open given file with given flags.
 * @returns 0 on success, -1 on failure.
 * @param pathname cannot be NULL, its length must be less than 108.
 * @param flags if "O_CREATE" is set, given file must not exist inside the storage and will thus be created; if "O_LOCK" is set,
 * the callee will ask for mutual exclusion over given file.
 * @exception It sets "errno" to "EINVAL" if any param is not valid, to "ENOTCONN" if callee is not connected to a socket, to
 * "EBADMSG" if any response read from the socket is not a valid one. The function may also fail and set "errno" for
 * any of the errors specified for the routines "writen", "readn", "Storage_openFile".
 * @note A fatal error may be triggered inside the storage when processing this request, therefore the callee may exit with
 * an exit status equal to the "errno" value set in the storage if "exit_on_fatal_errors" has been toggled on.
 * In order for the routine to succeed, the storage needs to have enough room for a new file and - if mutual exclusion over
 * it has been requested - the lock over the file must either be owned by callee or by none.
*/
int
openFile(const char* pathname, int flags);

/**
 * @brief Requests server to read given file; it also sets given pointers to its contents and its size.
 * @returns 0 on success, -1 on failure.
 * @param pathname cannot be NULL, its length must be less than 108.
 * @param buf can be NULL if size is NULL.
 * @param size can be NULL if buf is NULL.
 * @exception It sets "errno" to "EINVAL" if any param is not valid, to "ENOTCONN" if callee is not connected to a socket, to
 * "EBADMSG" if any response read from the socket is not a valid one. The function may also fail and set "errno" for
 * any of the errors specified for the routines "writen", "readn", "malloc", "Storage_readFile".
 * @note A fatal error may be triggered inside the storage when processing this request, therefore the callee may exit with
 * an exit status equal to the "errno" value set in the storage if "exit_on_fatal_errors" has been toggled on.
 * In order for a file to be read, it is required from the callee to have already opened it.
*/
int
readFile(const char* pathname, void** buf, size_t* size);

/**
 * @brief Requests server to read up to given param files.
 * @returns 0 on success, -1 on failure.
 * @param N cannot be negative; if it is 0 or greater than the number of files inside the storage, every file will be read.
 * @param dirname if NULL, read files will not be stored.
 * @exception It sets "errno" to "EINVAL" if any param is not valid, to "ENOTCONN" if callee is not connected to a socket, to
 * "EBADMSG" if any response read from the socket is not a valid one. The function may also fail and set "errno" for
 * any of the errors specified for the routines "writen", "readn", "malloc", "savefile", "Storage_readNFiles".
 * @note A fatal error may be triggered inside the storage when processing this request, therefore the callee may exit with
 * an exit status equal to the "errno" value set in the storage if "exit_on_fatal_errors" has been toggled on.
 * The only files this routine cannot access are the ones locked by other callees: it does not require the files to be opened by
 * callee.
*/
int
readNFiles(int N, const char* dirname);

/**
 * @brief Uploads given file to server.
 * @returns 0 on success, -1 on failure.
 * @param pathname cannot be NULL, its length must be less than 108.
 * @param dirname if NULL, files evicted because of this operation will not be stored.
 * @exception It sets "errno" to "EINVAL" if any param is not valid, to "ENOTCONN" if callee is not connected to a socket, to
 * "EBADMSG" if any response read from the socket is not a valid one. The function may also fail and set "errno" for
 * any of the errors specified for the routines "writen", "readn", "malloc", "savefile", "is_regular_file", "fopen",
 * "fseek", "fread", "Storage_writeFile".
 * @note A fatal error may be triggered inside the storage when processing this request, therefore the callee may exit with
 * an exit status equal to the "errno" value set in the storage if "exit_on_fatal_errors" has been toggled on.
 * In order for a file to be uploaded, it is required from the callee to have called "openFile" with flags "O_CREATE", "O_LOCK"
 * and no other successful operations to be performed on it before this routine is called.
*/
int
writeFile(const char* pathname, const char* dirname);

/**
 * @brief Requests server to append up to given size bytes to given file.
 * @returns 0 on success, -1 on failure.
 * @param pathname cannot be NULL, its length must be less than 108.
 * @param dirname if NULL, files evicted because of this operation will not be stored.
 * @exception It sets "errno" to "EINVAL" if any param is not valid, to "ENOTCONN" if callee is not connected to a socket, to
 * "EBADMSG" if any response read from the socket is not a valid one. The function may also fail and set "errno" for
 * any of the errors specified for the routines "writen", "readn", "malloc", "savefile", "Storage_appendToFile".
 * @note A fatal error may be triggered inside the storage when processing this request, therefore the callee may exit with
 * an exit status equal to the "errno" value set in the storage if "exit_on_fatal_errors" has been toggled on.
 * In order for the routine to succeed, the callee must have already opened this file and be able to access its contents.
*/
int
appendToFile(const char* pathname, void* buf, size_t size, const char* dirname);

/**
 * @brief Requests mutual exclusion over given file; if it is already in possession of another callee, the callee will
 * try again.
 * @returns 0 on success, -1 on failure.
 * @param pathname cannot be NULL, its length must be less than 108.
 * @exception It sets "errno" to "EINVAL" if any param is not valid, to "ENOTCONN" if callee is not connected to a socket, to
 * "EBADMSG" if any response read from the socket is not a valid one. The function may also fail and set "errno" for
 * any of the errors specified for the routines "writen", "readn", "Storage_lockFile".
 * @note A fatal error may be triggered inside the storage when processing this request, therefore the callee may exit with
 * an exit status equal to the "errno" value set in the storage if "exit_on_fatal_errors" has been toggled on.
 * In order for the routine to succeed, the callee must have already opened this file and the lock over it must be either
 * owned by the callee (before this routine's call) or by none.
*/
int
lockFile(const char* pathname);

/**
 * @brief Resets mutual exclusion over given file.
 * @returns 0 on success, -1 on failure.
 * @param pathname cannot be NULL, its length must be less than 108.
 * @exception It sets "errno" to "EINVAL" if any param is not valid, to "ENOTCONN" if callee is not connected to a socket, to
 * "EBADMSG" if any response read from the socket is not a valid one. The function may also fail and set "errno" for
 * any of the errors specified for the routines "writen", "readn", "Storage_unlockFile".
 * @note A fatal error may be triggered inside the storage when processing this request, therefore the callee may exit with
 * an exit status equal to the "errno" value set in the storage if "exit_on_fatal_errors" has been toggled on.
 * In order for the routine to succeed, the callee must own this file's lock.
*/
int
unlockFile(const char* pathname);

/**
 * @brief Requests server to close given file.
 * @returns 0 on success, -1 on failure.
 * @param pathname cannot be NULL, its length must be less than 108.
 * @exception It sets "errno" to "EINVAL" if any param is not valid, to "ENOTCONN" if callee is not connected to a socket, to
 * "EBADMSG" if any response read from the socket is not a valid one. The function may also fail and set "errno" for
 * any of the errors specified for the routines "writen", "readn", "Storage_closeFile".
 * @note A fatal error may be triggered inside the storage when processing this request, therefore the callee may exit with
 * an exit status equal to the "errno" value set in the storage if "exit_on_fatal_errors" has been toggled on.
 * In order for the routine to succeed, the callee must have opened this file.
*/
int
closeFile(const char* pathname);

/**
 * @brief Requests server to delete given file.
 * @returns 0 on success, -1 on failure.
 * @param pathname cannot be NULL, its length must be less than 108.
 * @exception It sets "errno" to "EINVAL" if any param is not valid, to "ENOTCONN" if callee is not connected to a socket, to
 * "EBADMSG" if any response read from the socket is not a valid one. The function may also fail and set "errno" for
 * any of the errors specified for the routines "writen", "readn", "Storage_removeFile".
 * @note A fatal error may be triggered inside the storage when processing this request, therefore the callee may exit with
 * an exit status equal to the "errno" value set in the storage if "exit_on_fatal_errors" has been toggled on.
 * In order for the routine to succeed, the callee must access this file in mutual exclusion.
*/
int
removeFile(const char* pathname);

#endif