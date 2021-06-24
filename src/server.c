#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>

#include <config.h>
#include <storage.h>
#include <wrappers.h>

#define MAXPATH 108
#define MAXCONNECTIONS 10

volatile sig_atomic_t hard_sig_received = 0;
volatile sig_atomic_t soft_sig_received = 0;

void signal_handler(int);

struct workers_args
{
	storage_t* storage;
	int pipe_output_channel;
};

int main(int argc, char* argv[])
{
	if (argc != 2)
	{
		fprintf(stderr, "Usage: %s <path-to-config.txt>", argv[0]);
		return 1;
	}
	int err; // placeholder for functions' output values
	int fd_socket; // socket's file descriptor
	int pipe_worker2manager[2]; // pipe used for worker-manager communications
	server_config_t* config = NULL; // server config
	storage_t* storage = NULL; // server storage
	struct sockaddr_un saddr; // socket address
	struct sigaction sig_action; sigset_t sigset;
	char* sockname = NULL; // socket's name
	pthread_t* workers = NULL; // worker threads pool
	unsigned long workers_pool_size = 0; // worker threads pool size
	fd_set master_read_set; // read set
	fd_set read_set; // copy of the original set
	int fd_num = 0;

	// --------------
	// handle signals:
	// --------------

	// per requirements: SIGPIPE is to be ignored
	sigaction(SIGPIPE, &(struct sigaction){SIG_IGN}, NULL);
	// per requirements: SIGINT, SIGQUIT, SIGHUP are to be explicitly handled
	memset(&sig_action, 0, sizeof(sig_action));
	sig_action.sa_handler = signal_handler;

	sigemptyset(&sigset);
	sigaddset(&sigset, SIGINT);
	sigaddset(&sigset, SIGHUP);
	sigaddset(&sigset, SIGQUIT);
	sig_action.sa_mask = sigset;

	EXIT_IF_NEQ(err, 0, sigaction(SIGINT, &sig_action, NULL), sigaction);
	EXIT_IF_NEQ(err, 0, sigaction(SIGHUP, &sig_action, NULL), sigaction);
	EXIT_IF_NEQ(err, 0, sigaction(SIGQUIT, &sig_action, NULL), sigaction);

	// ---------------------------
	// initialize server internals:
	// ---------------------------

	// initialize config file
	EXIT_IF_EQ(config, NULL, ServerConfig_Init(), ServerConfig_Init);
	// read config file
	// if config file is badly formatted the server shall not start
	EXIT_IF_NEQ(err, 0, ServerConfig_Set(config, argv[1]), ServerConfig_Set);

	// initialize server storage
	EXIT_IF_EQ(storage, NULL, Storage_Init((size_t) ServerConfig_GetMaxFilesNo(config),
				(size_t) ServerConfig_GetStorageSize(config), FIFO), Storage_Init);
	
	// initialize socket
	EXIT_IF_EQ(err, 0, ServerConfig_GetSocketFilePath(config, &sockname),
				ServerConfig_GetSocketFilePath);
	strncpy(saddr.sun_path, sockname, MAXPATH);
	saddr.sun_family = AF_UNIX;
	EXIT_IF_EQ(fd_socket, -1, socket(AF_UNIX, SOCK_STREAM, 0), socket);
	EXIT_IF_EQ(err, -1, bind(fd_socket, (struct sockaddr*)&saddr, sizeof saddr), bind);
	EXIT_IF_EQ(err, -1, listen(fd_socket, MAXCONNECTIONS), listen);

	// initialize pipe
	EXIT_IF_EQ(err, -1, pipe(pipe_worker2manager), pipe);

	// initialize fd set
	fd_num = MAX(fd_num, fd_socket);
	FD_ZERO(&master_read_set);
	FD_SET(fd_socket, &master_read_set);
	FD_SET(pipe_worker2manager[0], &master_read_set);

	EXIT_IF_EQ(workers_pool_size, 0, ServerConfig_GetWorkersNo(config),
				ServerConfig_GetWorkersNo);


	return 0;


	cleanup:
		ServerConfig_Free(config);
		Storage_Free(storage);
		unlink(sockname);
		free(sockname);
		close(pipe_worker2manager[0]);
		close(pipe_worker2manager[1]);
}

void signal_handler(int signum)
{
	switch (signum)
	{
		case SIGINT:
		case SIGQUIT:
			hard_sig_received = 1;
			break;

		case SIGHUP:
			soft_sig_received = 1;
	}
}