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
#include <server_defines.h>
#include <storage.h>
#include <utilities.h>
#include <wrappers.h>
#include <bounded_buffer.h>

#define MAXCONNECTIONS 10
#define PIPEBUFFERLEN 10
#define TASKLEN 32
#define REQUESTLEN 2048
#define MAXTASKS 4096

#define TERMINATE_WORKER 0 // used to send a termination message
#define CLIENT_LEFT "0"

#define NEXT_ITERATION \
	{ \
		free(fd_ready_string); \
		continue; \
	}

#define HANDLE_ANSWER \
{ \
	errnocopy = errno; \
	if (err == OP_SUCCESS) \
		fprintf(stdout, "[%s:%d] SUCCESS.\n", __FILE__, __LINE__); \
	memset(request, 0, REQUESTLEN); \
	snprintf(request, REQUESTLEN, "%d", err); \
	fflush(stdout); \
	EXIT_IF_EQ(tmp_err, -1, writen((long) fd_ready, (void*) request, REQUESTLEN), writen); \
	switch (err) \
	{ \
		case OP_SUCCESS: \
			break; \
		case OP_FAILURE: \
		case OP_FATAL: \
			memset(request, 0, REQUESTLEN); \
			snprintf(request, REQUESTLEN, "%d", errnocopy); \
			EXIT_IF_EQ(tmp_err, -1, writen((long) fd_ready, (void*) request, REQUESTLEN), writen); \
			break; \
	} \
}

#define REQUEST_DONE \
{ \
	memset(pipe_buffer, 0, PIPEBUFFERLEN); \
	snprintf(pipe_buffer, PIPEBUFFERLEN, "%d", fd_ready); \
	EXIT_IF_EQ(err, -1, writen((long) pipe_output_channel, (void*) pipe_buffer, PIPEBUFFERLEN), writen); \
	break; \
}

volatile sig_atomic_t terminate = 0;
volatile sig_atomic_t no_more_clients = 0;

static void* worker_routine(void*);
void signal_handler(int);

struct workers_args
{
	storage_t* storage;
	bounded_buffer_t* tasks;
	int pipe_output_channel;
};

int
main(int argc, char* argv[])
{
	if (argc != 2)
	{
		fprintf(stdout, "Usage: %s <path-to-config.txt>\n", argv[0]);
		return 1;
	}
	int err; // placeholder for functions' output values
	int fd_socket; // socket's file descriptor
	int fd_new_client; // new client's fd
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
	int fd_num = 0; // number of fds in set
	bounded_buffer_t* tasks = NULL; // used to store tasks to be done
	struct workers_args* workers_args = NULL; // worker threads' arguments
	size_t online_clients = 0; // number of clients currently online
	char pipe_buffer[PIPEBUFFERLEN]; // buffer used by pipe
	int pipe_msg; // message read from pipe
	char new_task[TASKLEN]; // used to denote task to be added as a string

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
	
	// initialize tasks' bounded buffer
	EXIT_IF_EQ(tasks, NULL, BoundedBuffer_Init(MAXTASKS), BoundedBuffer_Init);
	
	// initialize socket
	EXIT_IF_EQ(err, 0, ServerConfig_GetSocketFilePath(config, &sockname),
				ServerConfig_GetSocketFilePath);
	strncpy(saddr.sun_path, sockname, MAXPATH);
	saddr.sun_family = AF_UNIX;
	//fprintf(stdout, "sockname: %s\n", sockname);
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

	// -----------
	// main loop
	// -----------

	while (1)
	{
		if (terminate) goto cleanup;

		// fd set is to be reset as select does not
		// preserve it
		read_set = master_read_set;

		if ((select(fd_num + 1, &read_set, NULL, NULL, NULL)) == -1)
		{
			if (errno == EINTR)
			{
				if (no_more_clients && online_clients == 0) break;
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
		for (int i = 0; i < fd_num + 1; i++)
		{
			if (FD_ISSET(i, &read_set)) // i is ready
			{
				// reading from pipe means
				// worker is done with a request
				if (i == pipe_worker2manager[0])
				{
					EXIT_IF_EQ(err, -1, readn((long) i, (void*) pipe_buffer,
								PIPEBUFFERLEN), read);

					EXIT_IF_NEQ(err, 1, sscanf(pipe_buffer, "%d", &pipe_msg), sscanf);

					if (pipe_msg) // tmp != 0 means no client left
					{
						FD_SET(pipe_msg, &master_read_set);
						fd_num = MAX(pipe_msg, fd_num);
					}
					else // client left
					{
						online_clients--;
						if (online_clients == 0 && no_more_clients)
							goto cleanup;
					}
				}
				else if (i == fd_socket) // new client
				{
					EXIT_IF_EQ(fd_new_client, -1, accept(fd_socket, NULL, 0), accept);
					fprintf(stdout, "Accepted new client : %d\n", fd_new_client);

					if (no_more_clients) close(fd_new_client);
					else
					{
						FD_SET(fd_new_client, &master_read_set);
						online_clients++;
						fd_num = MAX(fd_new_client, fd_num);
					}
				}
				else // new task from client
				{
					memset(new_task, 0, TASKLEN);
					snprintf(new_task, TASKLEN, "%d", i);
					fprintf(stdout, "New task received from : %s\n", new_task);
					//FD_CLR(i, &master_read_set);
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
		snprintf(new_task, TASKLEN, "%d", TERMINATE_WORKER);
		for (size_t i = 0; i < (size_t) workers_pool_size; i++)
			EXIT_IF_NEQ(err, 0, BoundedBuffer_Enqueue(tasks, new_task),
						BoundedBuffer_Enqueue);
		for (size_t i = 0; i < (size_t) workers_pool_size; i++)
			pthread_join(workers[i], NULL);
		ServerConfig_Free(config);
		Storage_Print(storage);
		Storage_Free(storage);
		BoundedBuffer_Free(tasks);
		unlink(sockname);
		free(sockname);
		free(workers_args);
		free(workers);
		close(pipe_worker2manager[0]);
		close(pipe_worker2manager[1]);
}

/**
 * @brief All worker threads work following the very
 * same logic. At first, it is checked whether the descriptor
 * to read from is a valid one (i.e. it is not equal to 0);
 * then, it parses through the message read from the descriptor
 * (at this point, it is assumed all messages are valid).
 * @returns NULL.
 * 
*/
static void*
worker_routine(void* arg)
{
	struct workers_args* workers_args = (struct workers_args*) arg;
	bounded_buffer_t* tasks = workers_args->tasks;
	storage_t* storage = workers_args->storage;
	int pipe_output_channel = workers_args->pipe_output_channel;
	int err; // used as a placeholder for functions' output values
	int tmp_err; // used as a placeholder for functions' output values
	int errnocopy; // copy of errno value
	char* fd_ready_string; // currently being served client as a string
	char* token = NULL; // token for strtok_r
	char* saveptr = NULL; // saveptr for strtok_r
	int fd_ready; // currently being served client
	opcodes_t request_type; // type of request to be handled
	char* request; // request as a string
	char* tmp_request; // copy of request as a string
	EXIT_IF_EQ(request, NULL, (char*) malloc(sizeof(char) * REQUESTLEN), malloc);
	char pathname[MAXPATH]; // used to denote pathname for operations on storage
	linked_list_t* evicted = NULL; // used to store evicted files
	void* read_buf = NULL; // buffer used for reading operation
	size_t read_size = 0; // size of read_buf
	void* append_buf = NULL; // buffer used for append operation
	size_t append_size = 0; // size of append_buf
	int flags = 0; // used to denote flags for operations on storage
	char* evicted_file_name = NULL; // name of evicted file
	char* evicted_file_content = NULL; // content of evicted file
	char pipe_buffer[PIPEBUFFERLEN]; // buffer to be written on pipe
	while(1)
	{
		// reset task string
		fd_ready_string = NULL;
		// read ready fd from tasks' queue
		EXIT_IF_NEQ(err, 0, BoundedBuffer_Dequeue(tasks, &fd_ready_string),
					BoundedBuffer_Dequeue);
		fprintf(stdout, "ready fd: %s\n", fd_ready_string);
		EXIT_IF_NEQ(err, 1, sscanf(fd_ready_string, "%d", &fd_ready), sscanf);
		if (fd_ready == TERMINATE_WORKER) // termination message
		{
			free(fd_ready_string);
			break;
		}
		memset(request, 0, TASKLEN);
		EXIT_IF_EQ(err, -1, readn((long) fd_ready, (void*) request,
					REQUESTLEN), readn);
		// request now contains the operation to be run
		// and its arguments as a string
		tmp_request = request;
		token = strtok_r(tmp_request, " ", &saveptr);
		if (!token) NEXT_ITERATION;
		EXIT_IF_NEQ(err, 1, sscanf(token, "%d", (int*) &request_type), sscanf);
		fprintf(stdout, "request type = %d\n", (int) request_type);
		fflush(stdout);
		switch (request_type)
		{
			case OPEN:
				// get pathname
				memset(pathname, 0, MAXPATH);
				EXIT_IF_EQ(token, NULL, strtok_r(NULL, " ", &saveptr), strtok_r);
				EXIT_IF_NEQ(err, 1, sscanf(token, "%s", pathname), sscanf);
				// get flags
				flags = 0;
				EXIT_IF_EQ(token, NULL, strtok_r(NULL, " ", &saveptr), strtok_r);
				EXIT_IF_NEQ(err, 1, sscanf(token, "%d", &flags), sscanf);
				//fprintf(stderr, "pathname : %s\nflags : %d\nfd_ready : %d\n", pathname, flags, fd_ready);
				err = Storage_openFile(storage, pathname, flags, fd_ready);
				HANDLE_ANSWER;
				REQUEST_DONE;
				break;

			case CLOSE:
				// get pathname
				memset(pathname, 0, MAXPATH);
				EXIT_IF_EQ(token, NULL, strtok_r(NULL, " ", &saveptr), strtok_r);
				EXIT_IF_NEQ(err, 1, sscanf(token, "%s", pathname), sscanf);
				err = Storage_closeFile(storage, pathname, fd_ready);
				HANDLE_ANSWER;
				REQUEST_DONE;
				break;

			case READ:
				read_buf = NULL;
				read_size = 0;
				// get pathname
				memset(pathname, 0, MAXPATH);
				EXIT_IF_EQ(token, NULL, strtok_r(NULL, " ", &saveptr), strtok_r);
				EXIT_IF_NEQ(err, 1, sscanf(token, "%s", pathname), sscanf);
				// check whether file to be read's
				// size and contents are to be saved
				flags = 0;
				EXIT_IF_EQ(token, NULL, strtok_r(NULL, " ", &saveptr), strtok_r);
				EXIT_IF_NEQ(err, 1, sscanf(token, "%d", &flags), sscanf);
				if (flags == 1)
				{
					err = Storage_readFile(storage, pathname, &read_buf, &read_size, fd_ready);
					HANDLE_ANSWER;
					memset(request, 0, REQUESTLEN);
					snprintf(request, REQUESTLEN, "%lu", read_size);
					EXIT_IF_EQ(err, -1, writen((long) fd_ready, (void*) request,
								REQUESTLEN), writen);
					EXIT_IF_EQ(err, -1, writen((long) fd_ready, read_buf, read_size), writen);
					free(read_buf); read_buf = NULL;
				}
				else
				{
					err = Storage_readFile(storage, pathname, NULL, NULL, fd_ready);
					HANDLE_ANSWER;
				}
				// should somehow send read file
				REQUEST_DONE;
				break;

			case WRITE:
				evicted = NULL;
				// get pathname
				memset(pathname, 0, MAXPATH);
				EXIT_IF_EQ(token, NULL, strtok_r(NULL, " ", &saveptr), strtok_r);
				EXIT_IF_NEQ(err, 1, sscanf(token, "%s", pathname), sscanf);
				err = Storage_writeFile(storage, pathname, &evicted, fd_ready);
				HANDLE_ANSWER;
				// send number of victims
				memset(request, 0, REQUESTLEN);
				snprintf(request, REQUESTLEN, "%lu", LinkedList_GetNumberOfElements(evicted));
				EXIT_IF_EQ(err, -1, writen((long) fd_ready, (void*) request, REQUESTLEN), writen);
				// send evicted files
				if (LinkedList_GetNumberOfElements(evicted) != 0)
				{
					memset(request, 0, REQUESTLEN);
					snprintf(request, REQUESTLEN, "%lu", LinkedList_GetNumberOfElements(evicted));
					while (1)
					{
						if (LinkedList_GetNumberOfElements(evicted) == 0) break;
						EXIT_IF_EQ(err, -1, LinkedList_PopFront(evicted, &evicted_file_name,
									(void**) &evicted_file_content), LinkedList_PopFront);
						memset(request, 0, REQUESTLEN);
						// it is assumed file contents are null terminated
						snprintf(request, REQUESTLEN, "%lu:%s", strlen(evicted_file_content),
									evicted_file_name);
						EXIT_IF_EQ(err, -1, writen((long) fd_ready, (void*)
									request, REQUESTLEN), writen);
						fprintf(stderr, "[%d] File content is about to be sent.\n%s", __LINE__,
									evicted_file_content);
						EXIT_IF_EQ(err, -1, writen((long) fd_ready, evicted_file_content,
									strlen(evicted_file_content)), writen);
						free(evicted_file_name); evicted_file_name = NULL;
						free(evicted_file_content); evicted_file_content = NULL;
					}
				}
				LinkedList_Free(evicted); evicted = NULL;
				REQUEST_DONE;
				break;

			case APPEND:
				evicted = NULL;
				append_buf = NULL;
				append_size = 0;
				// get pathname
				memset(pathname, 0, MAXPATH);
				EXIT_IF_EQ(token, NULL, strtok_r(NULL, " ", &saveptr), strtok_r);
				EXIT_IF_NEQ(err, 1, sscanf(token, "%s", pathname), sscanf);
				// get buffer to append
				EXIT_IF_EQ(token, NULL, strtok_r(NULL, " ", &saveptr), strtok_r);
				EXIT_IF_NEQ(err, 1, sscanf(token, "%s", (char*) append_buf), sscanf);
				// get size of buffer
				EXIT_IF_EQ(token, NULL, strtok_r(NULL, " ", &saveptr), strtok_r);
				EXIT_IF_NEQ(err, 1, sscanf(token, "%lu", &append_size), sscanf);
				// check whether dirname has been specified
				token = strtok_r(NULL, " ", &saveptr);
				if (!token)
					err = Storage_appendToFile(storage, pathname, append_buf,
								append_size, NULL, fd_ready);
				else
					err = Storage_appendToFile(storage, pathname, append_buf,
								append_size, &evicted, fd_ready);
				// handle answer
				HANDLE_ANSWER;
				// send evicted files
				REQUEST_DONE;
				break;

			case READ_N:
				REQUEST_DONE;
				break;

			case LOCK:
				// get pathname
				memset(pathname, 0, MAXPATH);
				EXIT_IF_EQ(token, NULL, strtok_r(NULL, " ", &saveptr), strtok_r);
				EXIT_IF_NEQ(err, 1, sscanf(token, "%s", pathname), sscanf);
				err = Storage_lockFile(storage, pathname, fd_ready);
				HANDLE_ANSWER;
				REQUEST_DONE;
				break;

			case UNLOCK:
				// get pathname
				memset(pathname, 0, MAXPATH);
				EXIT_IF_EQ(token, NULL, strtok_r(NULL, " ", &saveptr), strtok_r);
				EXIT_IF_NEQ(err, 1, sscanf(token, "%s", pathname), sscanf);
				err = Storage_unlockFile(storage, pathname, fd_ready);
				HANDLE_ANSWER;
				REQUEST_DONE;
				break;

			case REMOVE:
				// get pathname
				memset(pathname, 0, MAXPATH);
				EXIT_IF_EQ(token, NULL, strtok_r(NULL, " ", &saveptr), strtok_r);
				EXIT_IF_NEQ(err, 1, sscanf(token, "%s", pathname), sscanf);
				err = Storage_removeFile(storage, pathname, fd_ready);
				HANDLE_ANSWER;
				REQUEST_DONE;
				break;
			/**
			case TERMINATE:
				EXIT_IF_EQ(err, -1, close(fd_ready), close);
				EXIT_IF_EQ(err, -1, writen(pipe_output_channel, CLIENT_LEFT,
							PIPEBUFFERLEN), writen);
				break;
			*/
		}
		free(fd_ready_string);
	}
	free(request);
	return NULL;
}

/**
 * @brief Signal handler.
*/
void
signal_handler(int signum)
{
	switch (signum)
	{
		case SIGINT:
		case SIGQUIT:
			terminate = 1;
			break;

		case SIGHUP:
			no_more_clients = 1;
			break;
		
		case SIGPIPE:
			break;
	}
}