/**
 * @brief Header file made for parsing config.txt file into a server config struct.
 * @author Giacomo Trapani.
*/

#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <stdlib.h>

#ifdef DEBUG
struct _server_config
{
	unsigned long
		workers_no,
		max_files_no,
		storage_size;
	char socket_path[MAXPATH];
	char log_path[MAXPATH];
};
#endif

// Struct fields are not exposed to forbid user from editing them
typedef struct _server_config server_config_t;

/**
 * @brief Initializes empty server config struct.
 * @returns Pointer to initialized struct on success, NULL on failure.
 * @exception It sets errno to ENOMEM if and only if required memory allocation fails.
*/
server_config_t*
ServerConfig_Init();

/**
 * @brief Sets struct fields according to what has been specified in given file.
 * @returns 0 on success, -1 on failure.
 * @exception It sets errno to EINVAL if and only if config is NULL or
 * config file is not valid, (it sets errno to ERANGE) if numerical values
 * specified in config file are not valid. As this function requires the file to be opened,
 * refer to fopen for any other errno value.
*/
int
ServerConfig_Set(server_config_t* config, const char* config_file_path);

/**
 * @brief Getter for number of workers.
 * @returns Number of workers on success, 0 on failure.
 * @exception It sets errno to EINVAL if and only if server config is NULL.
*/
unsigned long
ServerConfig_GetWorkersNo(const server_config_t* config);

/**
 * @brief Getter for maximum number of storable files.
 * @returns Maximum number of storable files on success, 0 on failure.
 * @exception It sets errno to EINVAL if and only if server config is NULL.
*/
unsigned long
ServerConfig_GetMaxFilesNo(const server_config_t* config);

/**
 * @brief Getter for storage size.
 * @returns Storage size on success, 0 on failure.
 * @exception It sets errno to EINVAL if and only if server config is NULL.
*/
unsigned long
ServerConfig_GetStorageSize(const server_config_t* config);

/**
 * @brief Getter for log file path. The second param needs to be a pointer to a non-allocated
 * char array.
 * @returns Length of the string identifying log file path on success, 0 on failure.
 * It sets second param to log file path.
 * @exception It sets errno to EINVAL if and only if server config or pointer to char array
 * is NULL, (it sets errno to) ENOMEM if and only if required memory allocation fails.
*/
unsigned long
ServerConfig_GetLogFilePath(const server_config_t* config, char** log_path_ptr);

/**
 * @brief Getter for socket file path. The second param needs to be a pointer to a non-allocated
 * char array.
 * @returns Length of the string identifying socket file path on success, 0 on failure.
 * It sets second param to socket file path.
 * @exception It sets errno to EINVAL if and only if server config or pointer to char array
 * is NULL, (it sets errno to) ENOMEM if and only if required memory allocation fails.
*/
unsigned long
ServerConfig_GetSocketFilePath(const server_config_t* config, char** socket_path_ptr);

/**
 * Frees allocated resources.
*/
void
ServerConfig_Free(server_config_t* config);

#endif