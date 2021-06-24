/**
 * @brief Server file.
 * @author Giacomo Trapani.
*/
#define _POSIX_C_SOURCE 200112L
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>

#include <config.h>
#include <storage.h>
#include <wrappers.h>
#include <bounded_buffer.h>

#define MAXPATH 108
#define MAXCONNECTIONS 10
#define PIPEBUFFERLEN 10
#define TASKLEN 256

volatile sig_atomic_t hard_sig_received = 0;
volatile sig_atomic_t soft_sig_received = 0;

void signal_handler(int);
static void* worker_routine(void*);

struct workers_args
{
	storage_t* storage;
	bounded_buffer_t* tasks;
	int pipe_output_channel;
};

int main(int argc, char* argv[])
{
	if (argc != 2)
	{
		fprintf(stderr, "Usage: %s <path-to-config.txt>\n", argv[0]);
		return 1;
	}
	int err; // placeholder for functions' output values
	int fd_socket; // socket's file descriptor
	int fd_new_client;
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
	bounded_buffer_t* tasks = NULL; // used to store tasks to be done
	struct workers_args* workers_args = NULL; // worker threads' arguments
	size_t online_clients = 0; // number of clients currently online
	char pipe_buffer[PIPEBUFFERLEN];
	int tmp;

	// --------------
	// handle signals:
	// --------------

	memset(&sig_action, 0, sizeof(sig_action));
	sig_action.sa_handler = signal_handler;

	sigemptyset(&sigset);
	sigaddset(&sigset, SIGINT);
	sigaddset(&sigset, SIGHUP);
	sigaddset(&sigset, SIGQUIT);
	sig_action.sa_mask = sigset;

	// per requirements: SIGINT, SIGQUIT, SIGHUP are to be explicitly handled
	EXIT_IF_NEQ(err, 0, sigaction(SIGINT, &sig_action, NULL), sigaction);
	EXIT_IF_NEQ(err, 0, sigaction(SIGHUP, &sig_action, NULL), sigaction);
	EXIT_IF_NEQ(err, 0, sigaction(SIGQUIT, &sig_action, NULL), sigaction);
	// per requirements: SIGPIPE is to be ignored
	EXIT_IF_NEQ(err, 0, sigaction(SIGPIPE, &sig_action, NULL), sigaction);

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
	//fprintf(stderr, "sockname: %s\n", sockname);
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

	// initialize workers' arguments
	EXIT_IF_EQ(workers_args, NULL, (struct workers_args*)
				malloc(sizeof(struct workers_args)), malloc);
	workers_args->storage = storage;
	workers_args->tasks = tasks;
	workers_args->pipe_output_channel = pipe_worker2manager[1];

	// initialize workers pool
	EXIT_IF_EQ(workers_pool_size, 0, ServerConfig_GetWorkersNo(config),
				ServerConfig_GetWorkersNo);
	EXIT_IF_EQ(workers, NULL, (pthread_t*)
				malloc(sizeof(pthread_t) * workers_pool_size), malloc);
	for (size_t i = 0; i < (size_t) workers_pool_size; i++)
		EXIT_IF_EQ(err, -1, pthread_create(&(workers[i]), NULL,
					&worker_routine, (void*) workers_args), pthread_create);


	while (1)
	{
		if (hard_sig_received) goto cleanup;

		// fd set is to be reset as select does not
		// preserve it
		read_set = master_read_set;

		if ((select(fd_num + 1, &read_set, NULL, NULL, NULL)) == -1)
		{
			if (errno == EINTR)
			{
				if (soft_sig_received && online_clients == 0) break;
				continue;
			}
			else
			{
				perror("select");
				return 1;
			}
		}

		// loop through the fd set
		// to find out which fd is now ready
		for (size_t i = 0; i < fd_num + 1; i++)
		{
			if (FD_ISSET(i, &read_set)) // i is ready
			{
				// reading from pipe means
				// worker is done with a request
				if (i == pipe_worker2manager[0])
				{
					EXIT_IF_EQ(err, -1, read(i, pipe_buffer, PIPEBUFFERLEN), read);

					tmp = atoi(pipe_buffer);
					if (tmp) // tmp != 0 means no client left
					{
						FD_SET(tmp, &master_read_set);
						fd_num = MAX(tmp, fd_num);
					}
					else // client left
					{
						online_clients--;
						if (online_clients == 0 && soft_sig_received)
							goto cleanup;
					}
				}
				else if (i == fd_socket) // new client
				{
					EXIT_IF_EQ(fd_new_client, -1, accept(fd_socket, NULL, 0), accept);

					if (soft_sig_received) close(fd_new_client);
					else
					{
						FD_SET(fd_new_client, &master_read_set);
						online_clients++;
						fd_num = MAX(fd_new_client, fd_num);
					}
				}
				else // new task from client
				{
					char new_task[TASKLEN];
					memset(new_task, 0, TASKLEN);
					snprintf(new_task, TASKLEN, "%lu", i);
					FD_CLR(i, &master_read_set);
					if (i == fd_num) fd_num--;
					// push ready file descriptor to task queue for workers
					EXIT_IF_EQ(err, -1, BoundedBuffer_Enqueue(tasks, new_task),
								BoundedBuffer_Enqueue);
				}
			}
		}
	}

	return 0;


	cleanup:
		ServerConfig_Free(config);
		Storage_Free(storage);
		unlink(sockname);
		free(sockname);
		free(workers_args);
		for (size_t i = 0; i < (size_t) workers_pool_size; i++)
			pthread_join(workers[i], NULL);
		free(workers);
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
			break;
		
		case SIGPIPE:
			break;
	}
}

static void* worker_routine(void* arg)
{
	return NULL;
}