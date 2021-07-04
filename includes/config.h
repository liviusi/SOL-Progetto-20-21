/**
 * @brief Header file made for parsing config.txt file into a server config struct.
 * @author Giacomo Trapani.
*/

#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <stdlib.h>

#include <server_defines.h>

// Struct fields are not exposed to force callee to access it using the implemented methods.
typedef struct _server_config server_config_t;

/**
 * @brief Initializes empty server config struct.
 * @returns Pointer to initialized struct on success,
 * NULL on failure.
 * @exception It sets "errno" for any of the errors specified for the routine "malloc".
*/
server_config_t*
ServerConfig_Init();

/**
 * @brief Sets struct fields according to what has been specified in given file.
 * @returns 0 on success, -1 on failure.
 * @param config cannot be NULL.
 * @param config_file_path cannot be NULL. Its contents need to abide a few rules.
 * @exception It sets "errno" to "EINVAL" if any param is not valid. The function may also fail and set "errno"
 * for any of the errors specified for the routines "fopen", "fgets", "strtoul".
*/
int
ServerConfig_Set(server_config_t* config, const char* config_file_path);

/**
 * @brief Gets number of workers.
 * @returns Number of workers on success, 0 on failure.
 * @param config cannot be NULL.
 * @exception It sets "errno" to "EINVAL" if any param is not valid.
*/
unsigned long
ServerConfig_GetWorkersNo(const server_config_t* config);

/**
 * @brief Gets maximum number of storable files.
 * @returns Maximum number of storable files on success, 0 on failure.
 * @param config cannot be NULL.
 * @exception It sets "errno" to "EINVAL" if any param is not valid.
*/
unsigned long
ServerConfig_GetMaxFilesNo(const server_config_t* config);

/**
 * @brief Gets maximum storage size.
 * @returns Maximum storage size on success, 0 on failure.
 * @param config cannot be NULL.
 * @exception It sets "errno" to "EINVAL" if any param is not valid.
*/
unsigned long
ServerConfig_GetStorageSize(const server_config_t* config);

/**
 * @brief Copies log file path to non-allocated buffer.
 * @returns Length of the string identifying log file path on success, 0 on failure.
 * @param config cannot be NULL.
 * @param log_path_ptr cannot be NULL.
 * @exception It sets "errno" to "EINVAL" if any param is not valid. The function may also fail and set "errno"
 * for any of the errors specified for the routine "malloc".
*/
unsigned long
ServerConfig_GetLogFilePath(const server_config_t* config, char** log_path_ptr);

/**
 * @brief Copies socket file path to non-allocated buffer.
 * @returns Length of the string identifying socket file path on success, 0 on failure.
 * @param config cannot be NULL.
 * @param socket_path_ptr cannot be NULL.
 * @exception It sets "errno" to "EINVAL" if any param is not valid. The function may also fail and set "errno"
 * for any of the errors specified for the routine "malloc".
*/
unsigned long
ServerConfig_GetSocketFilePath(const server_config_t* config, char** socket_path_ptr);

/**
 * @brief Gets replacement policy.
 * @returns Replacement policy on success, 0 on failure.
 * @param config cannot be NULL.
 * @exception It sets "errno" to "EINVAL" if any param is not valid.
*/
replacement_policy_t
ServerConfig_GetReplacementPolicy(const server_config_t* config);

/**
 * Frees allocated resources.
*/
void
ServerConfig_Free(server_config_t* config);

#endif