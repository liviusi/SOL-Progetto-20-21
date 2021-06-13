#include <config.h>
#include <stdio.h>

int main(int argc, char* argv[])
{
	if (argc != 2) return 1;
	char* log_path;
	char* socket_path;
	server_config_t* config = ServerConfig_Init();
	ServerConfig_Set(config, argv[1]);
	fprintf(stdout, "#workers : %lu\n", ServerConfig_GetWorkersNo(config));
	fprintf(stdout, "#storable : %lu\n", ServerConfig_GetMaxFilesNo(config));
	fprintf(stdout, "size(storage) : %lu\n", ServerConfig_GetStorageSize(config));
	ServerConfig_GetLogFilePath(config, &log_path);
	fprintf(stdout, "log_path : %s\n", log_path);
	ServerConfig_GetSocketFilePath(config, &socket_path);
	fprintf(stdout, "socket_path : %s\n", socket_path);
	ServerConfig_Free(config);
	free(log_path);
	free(socket_path);
}