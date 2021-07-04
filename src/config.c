/**
 * @brief Source file for config header.
 * @author Giacomo Trapani.
*/

#include <config.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <limits.h>

#include <server_defines.h>


#define PARAMS 6
#define BUFFERLEN 256
#define WORKERSNO "NUMBER OF THREAD WORKERS = "
#define MAXFILESNO "MAXIMUM NUMBER OF STORABLE FILES = "
#define STORAGESIZE "MAXIMUM STORAGE SIZE = "
#define SOCKETPATH "SOCKET FILE PATH = "
#define LOGPATH "LOG FILE PATH = "
#define CHOSENPOLICY "REPLACEMENT POLICY = "

struct _server_config
{
	unsigned long
		workers_no, // number of thread workers
		max_files_no, // maximum number of storable files
		storage_size; // maximum storage size
	char socket_path[MAXPATH]; // absolute path to socket file
	char log_path[MAXPATH]; // absolute path to log file
	replacement_policy_t policy;
};

server_config_t* ServerConfig_Init()
{
	server_config_t* config = (server_config_t*) malloc(sizeof(server_config_t));
	if (!config)
	{
		errno = ENOMEM;
		return NULL;
	}
	config->workers_no = 0;
	config->max_files_no = 0;
	config->storage_size = 0;
	memset(config->socket_path, 0, MAXPATH);
	memset(config->log_path, 0, MAXPATH);
	return config;
}

int
ServerConfig_Set(server_config_t* config, const char* config_file_path)
{
	if (!config || !config_file_path) goto invalid_config;

	FILE* config_file;
	if ((config_file = fopen(config_file_path, "r")) == NULL) return -1;

	size_t i = 0;
	char buffer[BUFFERLEN];
	char* dummy;
	bool
		flag_workers = false, flag_max = false, flag_storage = false,
		flag_socket = false, flag_log = false, flag_policy = false;
	unsigned long tmp;
	while (i < PARAMS)
	{
		dummy = fgets(buffer, BUFFERLEN, config_file);
		if (!dummy && !feof(config_file)) return -1;
		if (strncmp(buffer, WORKERSNO, strlen(WORKERSNO)) == 0)
		{
			if (!flag_workers) flag_workers = true;
			else goto invalid_config;
			tmp = strtoul(buffer + strlen(WORKERSNO), NULL, 10);
			if (tmp != 0) 
			{
				if (tmp == ULONG_MAX && errno == ERANGE) goto invalid_config;
				else
				{
					config->workers_no = tmp;
					i++;
					continue;
				}
			}
			else goto invalid_config;
		}
		if (strncmp(buffer, MAXFILESNO, strlen(MAXFILESNO)) == 0)
		{
			if (!flag_max) flag_max = true;
			else goto invalid_config;
			tmp = strtoul(buffer + strlen(MAXFILESNO), NULL, 10);
			if (tmp != 0) 
			{
				if (tmp == ULONG_MAX && errno == ERANGE) goto invalid_config;
				else
				{
					config->max_files_no = tmp;
					i++;
					continue;
				}
			}
			else goto invalid_config;
		}
		if (strncmp(buffer, STORAGESIZE, strlen(STORAGESIZE)) == 0)
		{
			if (!flag_storage) flag_storage = true;
			else goto invalid_config;
			tmp = strtoul(buffer + strlen(STORAGESIZE), NULL, 10);
			if (tmp != 0)
			{
				if (tmp == ULONG_MAX && errno == ERANGE) goto invalid_config;
				else
				{
					config->storage_size = tmp;
					i++;
					continue;
				}
			}
			else goto invalid_config;
		}
		if (strncmp(buffer, SOCKETPATH, strlen(SOCKETPATH)) == 0)
		{
			if (!flag_socket) flag_socket = true;
			else goto invalid_config;
			strncpy(config->socket_path, buffer + strlen(SOCKETPATH), MAXPATH);
			config->socket_path[strcspn(config->socket_path, "\n")] = '\0';
			i++;
			continue;
		}
		if (strncmp(buffer, LOGPATH, strlen(LOGPATH)) == 0)
		{
			if (!flag_log) flag_log = true;
			else goto invalid_config;
			strncpy(config->log_path, buffer + strlen(LOGPATH), MAXPATH);
			config->log_path[strcspn(config->log_path, "\n")] = '\0';
			i++;
			continue;
		}
		if (strncmp(buffer, CHOSENPOLICY, strlen(CHOSENPOLICY)) == 0)
		{
			if (!flag_policy) flag_policy = true;
			else goto invalid_config;
			tmp = strtoul(buffer + strlen(CHOSENPOLICY), NULL, 10);
			if (tmp <= 2) 
			{
				config->policy = tmp;
				i++;
				continue;
			}
			else goto invalid_config;
		}
	}
	if (fclose(config_file) != 0) return -1;
	return 0;

	invalid_config:
		config->workers_no = 0;
		config->max_files_no = 0;
		config->storage_size = 0;
		memset(config->socket_path, 0, MAXPATH);
		memset(config->log_path, 0, MAXPATH);
		fclose(config_file);
		errno = EINVAL;
		return -1;
}

unsigned long
ServerConfig_GetWorkersNo(const server_config_t* config)
{
	if (!config)
	{
		errno = EINVAL;
		return 1;
	}
	return config->workers_no;
}

unsigned long
ServerConfig_GetMaxFilesNo(const server_config_t* config)
{
	if (!config)
	{
		errno = EINVAL;
		return 1;
	}
	return config->max_files_no;
}

unsigned long
ServerConfig_GetStorageSize(const server_config_t* config)
{
	if (!config)
	{
		errno = EINVAL;
		return 1;
	}
	return config->storage_size;
}

unsigned long
ServerConfig_GetLogFilePath(const server_config_t* config, char** log_path_ptr)
{
	if (!config || !log_path_ptr)
	{
		errno = EINVAL;
		return 0;
	}
	char* tmp = (char*) malloc(sizeof(char) * MAXPATH);
	if (!tmp)
	{
		errno = ENOMEM;
		return 0;
	}
	strncpy(tmp, config->log_path, MAXPATH);
	*log_path_ptr = tmp;
	return strlen(tmp);
}


unsigned long
ServerConfig_GetSocketFilePath(const server_config_t* config, char** socket_path_ptr)
{
	if (!config || !socket_path_ptr)
	{
		errno = EINVAL;
		return 0;
	}
	char* tmp = (char*) malloc(sizeof(char) * MAXPATH);
	if (!tmp)
	{
		errno = ENOMEM;
		return 0;
	}
	strncpy(tmp, config->socket_path, MAXPATH);
	*socket_path_ptr = tmp;
	return strlen(tmp);
}

replacement_policy_t
ServerConfig_GetReplacementPolicy(const server_config_t* config)
{
	if (!config)
	{
		errno = EINVAL;
		return 0;
	}
	return config->policy;
}

void
ServerConfig_Free(server_config_t* config)
{
	free(config);
	return;
}