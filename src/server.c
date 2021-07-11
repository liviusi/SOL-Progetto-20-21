/**
 * @brief Server file.
 * @author Giacomo Trapani.
*/
#define _POSIX_C_SOURCE 200112L
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <pthread.h>
#include <unistd.h>

#include <bounded_buffer.h>
#include <config.h>
#include <server_defines.h>
#include <storage.h>
#include <utilities.h>
#include <wrappers.h>

#define MAXCONNECTIONS 10
#define PIPEBUFFERLEN 10
#define TASKLEN 32
#define MAXTASKS 4096

#define TERMINATE_WORKER 0 // used to send a termination message
#define CLIENT_LEFT "0"

/**
 * Used in worker routine to proceed to next iteration.
*/
#define NEXT_ITERATION \
{ \
	free(fd_ready_string); \
	continue; \
}

/**
 * Used in worker routine to send a completion message as soon as a worker is done with a task.
*/
#define REQUEST_DONE \
{ \
	memset(pipe_buffer, 0, PIPEBUFFERLEN); \
	snprintf(pipe_buffer, PIPEBUFFERLEN, "%d", fd_ready); \
	EXIT_IF_EQ(err, -1, writen((long) pipe_output_channel, (void*) pipe_buffer, PIPEBUFFERLEN), writen); \
	break; \
}

/**
 * Used to write to log in mutual exclusion.
*/
#define LOG_EVENT(...) \
do \
{ \
	if (pthread_mutex_lock(&log_mutex) != 0) { perror("pthread_mutex_lock"); exit(1); } \
	fprintf(log_file, __VA_ARGS__); \
	if (pthread_mutex_unlock(&log_mutex) != 0) { perror("pthread_mutex_unlock"); exit(1); } \
} while(0);

volatile sig_atomic_t terminate = 0; // toggled on when server should terminate as soon as possible
volatile sig_atomic_t no_more_clients = 0; // toggled on when server must not accept any other client

pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER; // mutex for log file

/**
 * @brief All worker threads work following the very same logic. At first, it is checked whether the descriptor
 * to read from is a valid one (i.e. it is not equal to 0); then, it parses through the message read from the descriptor.
 * @returns NULL.
 * @note IT IS ASSUMED ALL MESSAGES ARE SENT THROUGH THE API AND THUS VALID.
*/
static void*
worker_routine(void*);

/**
 * @brief Used to handle signals according to requirements.
 * @returns NULL.
*/
static void*
signal_handler_routine(void*);

/**
 * Used to give each worker thread the needed arguments in order to communicate with the
 * implemented filesystem and the server. It also allows them to log events.
*/
struct workers_args
{
	storage_t* storage;
	bounded_buffer_t* tasks;
	int pipe_output_channel;
	FILE* log_file;
};

int
main(int argc, char* argv[])
{
	if (argc != 2)
	{
		fprintf(stdout, "Usage: %s <path-to-config.txt>\n", argv[0]);
		return 1;
	}

	// -------------
	// DECLARATIONS
	// -------------
	int err; // placeholder for functions' output values
	int fd_socket = -1; // socket's file descriptor
	int fd_new_client = -1; // new client's fd
	int pipe_worker2manager[2]; // pipe used for worker-manager communications
	bool pipe_init = false; // toggled on if pipe has been initialized
	server_config_t* config = NULL; // server config
	storage_t* storage = NULL; // server storage
	struct sockaddr_un saddr; // socket address
	struct sigaction sig_action; sigset_t sigset; // signal mask
	char* sockname = NULL; // socket's name
	pthread_t* workers = NULL; // worker threads pool
	pthread_t signal_handler_thread; // signal handler's thread id
	bool signal_handler_created = false; // toggled on when signal handler thread has been created
	unsigned long workers_pool_size = 0; // worker threads pool size
	fd_set master_read_set; // read set
	fd_set read_set; // copy of the original set
	int fd_num = 0; // number of fds in set
	struct timeval timeout_master = { 0, 100000 }; // 100ms timeout used to check whether select has gotten stuck
	struct timeval timeout_copy; // used to preserve timeout value
	bounded_buffer_t* tasks = NULL; // used to store tasks to be done
	struct workers_args* workers_args = NULL; // worker threads' arguments
	size_t online_clients = 0; // number of clients currently online
	char pipe_buffer[PIPEBUFFERLEN]; // buffer used by pipe
	int pipe_msg; // message read from pipe
	char new_task[TASKLEN]; // used to denote task to be added as a string
	char* log_name = NULL; // name of log file
	FILE* log_file = NULL; // log as a FILE*
	size_t i = 0; // index in loops

	// ----------------
	// SIGNAL HANDLING
	// ----------------

	memset(&sig_action, 0, sizeof(sig_action));
	sig_action.sa_handler = SIG_IGN;

	sigemptyset(&sigset);
	sigaddset(&sigset, SIGINT);
	sigaddset(&sigset, SIGHUP);
	sigaddset(&sigset, SIGQUIT);
	sig_action.sa_mask = sigset;

	// per requirements: SIGPIPE is to be ignored
	EXIT_IF_NEQ(err, 0, sigaction(SIGPIPE, &sig_action, NULL), sigaction);
	// use dedicated thread to handle signals
	EXIT_IF_NEQ(err, 0, pthread_sigmask(SIG_BLOCK, &sigset, NULL), pthread_sigmask);
	EXIT_IF_NEQ(err, 0, pthread_create(&signal_handler_thread, NULL, &signal_handler_routine, (void*) &sigset), pthread_create);
	signal_handler_created = true;

	// ---------------------------------
	// SERVER INTERNALS' INITIALIZATION
	// ---------------------------------

	// initialize config file
	config = ServerConfig_Init();
	if (!config)
	{
		perror("ServerConfig_Init");
		goto failure;
	}
	// read config file
	// if config file is badly formatted the server shall not start
	err = ServerConfig_Set(config, argv[1]);
	if (err == -1)
	{
		perror("ServerConfig_Set");
		goto failure;
	}

	// initialize server storage
	storage = Storage_Init((size_t) ServerConfig_GetMaxFilesNo(config), (size_t) ServerConfig_GetStorageSize(config),
				ServerConfig_GetReplacementPolicy(config));
	if (!storage)
	{
		perror("Storage_Init");
		goto failure;
	}
	
	// initialize tasks' bounded buffer
	tasks = BoundedBuffer_Init(MAXTASKS);
	if (!tasks)
	{
		perror("BoundedBuffer_Init");
		goto failure;
	}
	
	// initialize socket
	err = ServerConfig_GetSocketFilePath(config, &sockname);
	if (err == 0)
	{
		perror("ServerConfig_GetSocketFilePath");
		goto failure;
	}
	strncpy(saddr.sun_path, sockname, MAXPATH);
	saddr.sun_family = AF_UNIX;
	//fprintf(stdout, "sockname: %s\n", sockname);
	fd_socket = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd_socket == -1)
	{
		perror("socket");
		goto failure;
	}
	err = bind(fd_socket, (struct sockaddr*)&saddr, sizeof saddr);
	if (err == -1)
	{
		perror("bind");
		goto failure;
	}
	err = listen(fd_socket, MAXCONNECTIONS);
	if (err == -1)
	{
		perror("listen");
		goto failure;
	}

	// initialize pipe
	err = pipe(pipe_worker2manager);
	if (err == -1)
	{
		perror("pipe");
		goto failure;
	}
	pipe_init = true;

	// initialize fd set
	fd_num = MAX(fd_num, fd_socket);
	FD_ZERO(&master_read_set);
	FD_SET(fd_socket, &master_read_set);
	FD_SET(pipe_worker2manager[0], &master_read_set);

	// initialize log file
	err = (int) ServerConfig_GetLogFilePath(config, &log_name);
	if (err == 0)
	{
		perror("ServerConfig_GetLogFilePath");
		goto failure;
	}
	mode_t oldmask = umask(033);
	log_file = fopen(log_name, "w+");
	if (!log_file)
	{
		perror("fopen");
		goto failure;
	}
	umask(oldmask);

	// initialize workers' arguments
	workers_args = (struct workers_args*) malloc(sizeof(struct workers_args));
	if (!workers_args)
	{
		perror("malloc");
		goto failure;
	}
	workers_args->storage = storage;
	workers_args->tasks = tasks;
	workers_args->pipe_output_channel = pipe_worker2manager[1];
	workers_args->log_file = log_file;

	// initialize workers pool
	workers_pool_size = ServerConfig_GetWorkersNo(config); // cannot fail
	workers = (pthread_t*) malloc(sizeof(pthread_t) * workers_pool_size);
	if (!workers)
	{
		perror("malloc");
		goto failure;
	}
	for (i = 0; i < (size_t) workers_pool_size; i++)
		{
			err = pthread_create(&(workers[i]), NULL, &worker_routine, (void*) workers_args);
			if (err != 0)
			{
				perror("pthread_create");
				goto failure;
			}
		}

	/**
	 * From this point, exceptions will be handled by exiting.
	*/

	// ---------
	// MAIN LOOP
	// ---------

	while (1)
	{
		if (terminate) goto cleanup;

		if (online_clients == 0 && no_more_clients) goto cleanup;

		// fd set is to be reset as select does not preserve it
		read_set = master_read_set;
		timeout_copy = timeout_master; // select does not preserve timeout value
		// A timeout is used not to be undefinitely locked on select
		if ((select(fd_num + 1, &read_set, NULL, NULL, &timeout_copy)) == -1)
		{
			if (errno != EINTR)
			{
				perror("select");
				exit(EXIT_FAILURE);
			}
			else
			{
				if (online_clients == 0 && no_more_clients) break;
				else if (terminate) goto cleanup;
				else continue;
			}
		}

		// loop through the fd set to find out which fd is now ready
		for (int j = 0; j < fd_num + 1; j++)
		{
			if (FD_ISSET(j, &read_set)) // i is ready
			{
				// reading from pipe means worker is done with a request
				if (j == pipe_worker2manager[0])
				{
					EXIT_IF_EQ(err, -1, readn((long) j, (void*) pipe_buffer, PIPEBUFFERLEN), readn);
					EXIT_IF_NEQ(err, 1, sscanf(pipe_buffer, "%d", &pipe_msg), sscanf);
					if (pipe_msg) // msg != 0 means no client left
					{
						FD_SET(pipe_msg, &master_read_set);
						fd_num = MAX(pipe_msg, fd_num);
					}
					else // client left
					{
						if (online_clients) online_clients--;
						if (online_clients == 0 && no_more_clients) break;
					}
				}
				else if (j == fd_socket) // new client
				{
					EXIT_IF_EQ(fd_new_client, -1, accept(fd_socket, NULL, 0), accept);
					if (no_more_clients) close(fd_new_client);
					else
					{
						LOG_EVENT("New client accepted : %d.\n", fd_new_client);
						FD_SET(fd_new_client, &master_read_set);
						online_clients++;
						LOG_EVENT("Current online clients : %lu.\n", online_clients);
						fd_num = MAX(fd_new_client, fd_num);
					}
				}
				else // new task from client
				{
					memset(new_task, 0, TASKLEN);
					snprintf(new_task, TASKLEN, "%d", j);
					FD_CLR(j, &master_read_set);
					if (j == fd_num) fd_num--;
					// push ready file descriptor to task queue for workers
					EXIT_IF_EQ(err, -1, BoundedBuffer_Enqueue(tasks, new_task), BoundedBuffer_Enqueue);
				}
			}
		}
	}

	return 0;


	cleanup:
		snprintf(new_task, TASKLEN, "%d", TERMINATE_WORKER);
		for (size_t j = 0; j < (size_t) workers_pool_size; j++)
			EXIT_IF_NEQ(err, 0, BoundedBuffer_Enqueue(tasks, new_task), BoundedBuffer_Enqueue);
		for (size_t j = 0; j < (size_t) workers_pool_size; j++)
			pthread_join(workers[j], NULL);
		pthread_join(signal_handler_thread, NULL);
		ServerConfig_Free(config);
		Storage_Print(storage);
		if (log_file)
		{
			LOG_EVENT("Maximum size reached : %5f.\n", Storage_GetReachedSize(storage) * MBYTE);
			LOG_EVENT("Maximum file number : %lu.\n", Storage_GetReachedFiles(storage));
		}
		Storage_Free(storage);
		BoundedBuffer_Free(tasks);
		if (sockname) { unlink(sockname); free(sockname); }
		free(log_name);
		free(workers_args);
		free(workers);
		if (pipe_init) { close(pipe_worker2manager[0]); close(pipe_worker2manager[1]); }
		if (log_file) fclose(log_file);
		if (fd_socket != -1) close(fd_socket);
		return 0;


	failure:
		if (workers)
		{
			size_t j = 0;
			while (j != i)
			{
				pthread_kill(workers[j], SIGKILL); // cannot fail
				j++;
			}
		}
		free(workers);
		if (signal_handler_created) pthread_kill(signal_handler_thread, SIGKILL);
		ServerConfig_Free(config);
		Storage_Free(storage);
		BoundedBuffer_Free(tasks);
		if (sockname) { unlink(sockname); free(sockname); }
		if (fd_socket != -1) close(fd_socket);
		if (log_file) fclose(log_file);
		if (pipe_init) { close(pipe_worker2manager[0]); close(pipe_worker2manager[1]); }
		free(log_name);
		free(workers_args);
		exit(EXIT_FAILURE);
}

static void*
signal_handler_routine(void* arg)
{
	sigset_t* set = (sigset_t*) arg; // used to denote signal set
	int err; // placeholder for functions' output values
	int sig; // used to denote received signal
	while (1)
	{
		EXIT_IF_NEQ(err, 0, sigwait(set, &sig), sigwait);
		switch (sig)
		{
			case SIGINT:
			case SIGQUIT:
				terminate = 1;
				return NULL;

			case SIGHUP:
				no_more_clients = 1;
				return NULL;

			default:
				break;
		}
	}
}

static void*
worker_routine(void* arg)
{
	// -------------------------------------
	// DECLARATIONS NEEDED TO PARSE MESSAGES
	// -------------------------------------
	char* request; // request as a string
	EXIT_IF_EQ(request, NULL, (char*) malloc(sizeof(char) * REQUESTLEN), malloc);
	struct workers_args* workers_args = (struct workers_args*) arg;
	bounded_buffer_t* tasks = workers_args->tasks;
	storage_t* storage = workers_args->storage;
	FILE* log_file = workers_args->log_file;
	int pipe_output_channel = workers_args->pipe_output_channel;
	int err; // used as a placeholder for functions' output values
	int tmp_err; // used as a placeholder for functions' output values
	int errnocopy; // copy of errno value
	char* fd_ready_string; // currently being served client as a string
	char* token = NULL; // token for strtok_r
	char* saveptr = NULL; // saveptr for strtok_r
	int fd_ready; // currently being served client
	opcodes_t request_type; // type of request to be handled
	char* tmp_request; // copy of request as a string
	char pathname[REQUESTLEN]; // used to denote pathname for operations on storage
	char pipe_buffer[PIPEBUFFERLEN]; // buffer to be written on pipe
	char msg_size[SIZELEN]; // used when reading or sending the length of the following message

	// --------------------------------------------
	// DECLARATIONS NEEDED TO INTERACT WITH STORAGE
	// --------------------------------------------
	linked_list_t* evicted = NULL; // used to store evicted files
	char* evicted_file_name = NULL; // name of evicted file
	char* evicted_file_content = NULL; // content of evicted file
	size_t evicted_file_size = 0; // size of evicted file content
	void* read_buf = NULL; // buffer used for reading operation (USED TO HANDLE readFile)
	size_t read_size = 0; // size of read_buf (USED TO HANDLE readFile)
	void* append_buf = NULL; // buffer used for append operation (USED TO HANDLE appendToFile)
	size_t append_size = 0; // size of buffer to be appended (USED TO HANDLE appendToFile)
	int flags = 0; // used to denote flags for operations on storage (USED TO HANDLE openFile)
	size_t write_size = 0; // used to denote size of contents to be written (USED TO HANDLE writeFile)
	char* write_contents = NULL; // buffer of contents to be written (USED TO HANDLE writeFILE)
	linked_list_t* read_files = NULL; // list of files read when interacting with a readNFiles request (USED TO HANDLE readNFiles)
	char* read_file_name = NULL; // used to denote name of read file (USED TO HANDLE readNFiles)
	char* read_file_content = NULL; // buffer of read file's contents (USED TO HANDLE readNFiles)
	size_t read_file_size = 0; // size of read file content (USED TO HANDLE readNFiles)
	size_t tot_read_size = 0; // total read size
	size_t N = 0; // number of files to be read (USED TO HANDLE readNFiles)
	while(1)
	{
		// reset task string
		fd_ready_string = NULL;
		// read ready fd from tasks' queue
		EXIT_IF_NEQ(err, 0, BoundedBuffer_Dequeue(tasks, &fd_ready_string), BoundedBuffer_Dequeue);
		EXIT_IF_NEQ(err, 1, sscanf(fd_ready_string, "%d", &fd_ready), sscanf);
		if (fd_ready == TERMINATE_WORKER) // termination message
		{
			free(fd_ready_string);
			break;
		}
		memset(request, 0, TASKLEN);
		EXIT_IF_EQ(err, -1, readn((long) fd_ready, (void*) request, REQUESTLEN), readn);
		// request now contains the operation to be run and its arguments as a string
		tmp_request = request;
		token = strtok_r(tmp_request, " ", &saveptr);
		if (!token) NEXT_ITERATION;
		EXIT_IF_NEQ(err, 1, sscanf(token, "%d", (int*) &request_type), sscanf);
		switch (request_type)
		{
			case OPEN:
				// get pathname
				memset(pathname, 0, REQUESTLEN);
				EXIT_IF_EQ(token, NULL, strtok_r(NULL, " ", &saveptr), strtok_r);
				EXIT_IF_NEQ(err, 1, sscanf(token, "%s", pathname), sscanf);
				flags = 0;
				EXIT_IF_EQ(token, NULL, strtok_r(NULL, " ", &saveptr), strtok_r);
				EXIT_IF_NEQ(err, 1, sscanf(token, "%d", &flags), sscanf);
				err = Storage_openFile(storage, pathname, flags, fd_ready);
				errnocopy = errno;
				// send return value
				memset(request, 0, REQUESTLEN);
				snprintf(request, REQUESTLEN, "%d", err);
				LOG_EVENT("[%d] openFile %s %d : %d.\n", (int) pthread_self(), pathname, flags, err);
				EXIT_IF_EQ(tmp_err, -1, writen((long) fd_ready, (void*) request, strlen(request) + 1), writen);
				switch (err)
				{
					case OP_SUCCESS:
						break;

					case OP_FAILURE:
						memset(request, 0, REQUESTLEN);
						snprintf(request, REQUESTLEN, "%d", errnocopy);
						EXIT_IF_EQ(err, -1, writen((long) fd_ready, (void*) request, ERRNOLEN), writen);
						break;

					case OP_FATAL:
						memset(request, 0, REQUESTLEN);
						snprintf(request, REQUESTLEN, "%d", errnocopy);
						EXIT_IF_EQ(err, -1, writen((long) fd_ready, (void*) request, ERRNOLEN), writen);
						exit(1);
				}
				flags = 0;
				REQUEST_DONE;
				break;

			case CLOSE:
				// get pathname
				memset(pathname, 0, REQUESTLEN);
				EXIT_IF_EQ(token, NULL, strtok_r(NULL, " ", &saveptr), strtok_r);
				EXIT_IF_NEQ(err, 1, sscanf(token, "%s", pathname), sscanf);
				err = Storage_closeFile(storage, pathname, fd_ready);
				errnocopy = errno;
				// send return value
				memset(request, 0, REQUESTLEN);
				snprintf(request, REQUESTLEN, "%d", err);
				LOG_EVENT("[%d] closeFile %s : %d.\n", (int) pthread_self(), pathname, err);
				EXIT_IF_EQ(tmp_err, -1, writen((long) fd_ready, (void*) request, strlen(request) + 1), writen);
				switch (err)
				{
					case OP_SUCCESS:
						break;

					case OP_FAILURE:
						memset(request, 0, REQUESTLEN);
						snprintf(request, REQUESTLEN, "%d", errnocopy);
						EXIT_IF_EQ(err, -1, writen((long) fd_ready, (void*) request, ERRNOLEN), writen);
						break;

					case OP_FATAL:
						memset(request, 0, REQUESTLEN);
						snprintf(request, REQUESTLEN, "%d", errnocopy);
						EXIT_IF_EQ(err, -1, writen((long) fd_ready, (void*) request, ERRNOLEN), writen);
						exit(1);
				}
				REQUEST_DONE;
				break;

			case READ:
				read_buf = NULL;
				read_size = 0;
				// get pathname
				memset(pathname, 0, REQUESTLEN);
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
					errnocopy = errno;
					// send return value
					memset(request, 0, REQUESTLEN);
					snprintf(request, REQUESTLEN, "%d", err);
					LOG_EVENT("[%d] readFile %s : %d -> %lu.\n", (int) pthread_self(), pathname, err, read_size);
					EXIT_IF_EQ(tmp_err, -1, writen((long) fd_ready, (void*) request, strlen(request) + 1), writen);
					switch (err)
					{
						case OP_SUCCESS:
							break;

						case OP_FAILURE:
							memset(request, 0, REQUESTLEN);
							snprintf(request, REQUESTLEN, "%d", errnocopy);
							EXIT_IF_EQ(err, -1, writen((long) fd_ready, (void*) request, ERRNOLEN), writen);
							break;

						case OP_FATAL:
							memset(request, 0, REQUESTLEN);
							snprintf(request, REQUESTLEN, "%d", errnocopy);
							EXIT_IF_EQ(err, -1, writen((long) fd_ready, (void*) request, ERRNOLEN), writen);
							exit(1);
					}
					// send size
					memset(msg_size, 0, SIZELEN);
					snprintf(msg_size, SIZELEN, "%lu", read_size);
					EXIT_IF_EQ(err, -1, writen((long) fd_ready, (void*) msg_size, SIZELEN), writen);
					if (read_size != 0)
						EXIT_IF_EQ(err, -1, writen((long) fd_ready, read_buf, read_size), writen);
					free(read_buf); read_buf = NULL;
				}
				else
				{
					err = Storage_readFile(storage, pathname, NULL, NULL, fd_ready);
					errnocopy = errno;
					// send return value
					memset(request, 0, REQUESTLEN);
					snprintf(request, REQUESTLEN, "%d", err);
					LOG_EVENT("[%d] readFile %s NULL: %d -> %lu.\n", (int) pthread_self(), pathname, err, read_size);
					EXIT_IF_EQ(tmp_err, -1, writen((long) fd_ready, (void*) request, strlen(request) + 1), writen);
					switch (err)
					{
						case OP_SUCCESS:
							break;

						case OP_FAILURE:
							memset(request, 0, REQUESTLEN);
							snprintf(request, REQUESTLEN, "%d", errnocopy);
							EXIT_IF_EQ(err, -1, writen((long) fd_ready, (void*) request, ERRNOLEN), writen);
							break;

						case OP_FATAL:
							memset(request, 0, REQUESTLEN);
							snprintf(request, REQUESTLEN, "%d", errnocopy);
							EXIT_IF_EQ(err, -1, writen((long) fd_ready, (void*) request, ERRNOLEN), writen);
							exit(1);
					}
				}
				flags = 0;
				REQUEST_DONE;
				break;

			case WRITE:
				evicted = NULL;
				evicted_file_name = NULL;
				evicted_file_content = NULL;
				write_contents = NULL;
				// get pathname
				memset(pathname, 0, REQUESTLEN);
				EXIT_IF_EQ(token, NULL, strtok_r(NULL, " ", &saveptr), strtok_r);
				EXIT_IF_NEQ(err, 1, sscanf(token, "%s", pathname), sscanf);
				// get content size
				EXIT_IF_EQ(token, NULL, strtok_r(NULL, " ", &saveptr), strtok_r);
				EXIT_IF_NEQ(err, 1, sscanf(token, "%lu", &write_size), sscanf);
				// allocate enough memory for contents
				if (write_size != 0)
				{
					EXIT_IF_EQ(write_contents, NULL, (char*) malloc(write_size + 1), malloc);
					memset(write_contents, 0, write_size + 1);
					EXIT_IF_EQ(err, -1, readn((long) fd_ready, (void*) write_contents, write_size), readn);
				}
				err = Storage_writeFile(storage, pathname, write_size, write_contents, &evicted, fd_ready);
				errnocopy = errno;
				free(write_contents);
				// send return value
				memset(request, 0, REQUESTLEN);
				snprintf(request, REQUESTLEN, "%d", err);
				LOG_EVENT("[%d] writeFile %s : %d -> %lu.\n\tVictims : %lu.\n", (int) pthread_self(), pathname, err,
							write_size, LinkedList_GetNumberOfElements(evicted));
				EXIT_IF_EQ(tmp_err, -1, writen((long) fd_ready, (void*) request, strlen(request) + 1), writen);
				switch (err)
				{
					case OP_SUCCESS:
						break;

					case OP_FAILURE:
						memset(request, 0, REQUESTLEN);
						snprintf(request, REQUESTLEN, "%d", errnocopy);
						EXIT_IF_EQ(err, -1, writen((long) fd_ready, (void*) request, ERRNOLEN), writen);
						break;

					case OP_FATAL:
						memset(request, 0, REQUESTLEN);
						snprintf(request, REQUESTLEN, "%d", errnocopy);
						EXIT_IF_EQ(err, -1, writen((long) fd_ready, (void*) request, ERRNOLEN), writen);
						break;
				}
				// send number of victims
				memset(msg_size, 0, SIZELEN);
				snprintf(msg_size, SIZELEN, "%lu", LinkedList_GetNumberOfElements(evicted));
				EXIT_IF_EQ(tmp_err, -1, writen((long) fd_ready, (void*) msg_size, SIZELEN), writen);
				// send victims if any
				while (LinkedList_GetNumberOfElements(evicted) != 0)
				{
					errno = 0;
					evicted_file_size = LinkedList_PopFront(evicted, &evicted_file_name, (void**) &evicted_file_content);
					if (evicted_file_size == 0 && errno == ENOMEM) exit(1);
					memset(request, 0, REQUESTLEN);
					snprintf(request, REQUESTLEN, "%s", evicted_file_name); // should error handle this
					// send victim's name
					EXIT_IF_EQ(tmp_err, -1, writen((long) fd_ready, (void*) request, REQUESTLEN), writen);
					LOG_EVENT("\tVictim name: %s.\n", evicted_file_name);
					// send victim's contents size
					memset(msg_size, 0, SIZELEN);
					snprintf(msg_size, SIZELEN, "%lu", evicted_file_size);
					EXIT_IF_EQ(tmp_err, -1, writen((long) fd_ready, (void*) msg_size, SIZELEN), writen);
					// send actual contents
					EXIT_IF_EQ(tmp_err, -1, writen((long) fd_ready, (void*) evicted_file_content, evicted_file_size), writen);
					free(evicted_file_name); evicted_file_name = NULL;
					free(evicted_file_content); evicted_file_content = NULL;
				}
				LinkedList_Free(evicted); evicted = NULL;
				if (err == OP_FATAL) exit(1);
				REQUEST_DONE;
				break;

			case APPEND:
				evicted = NULL;
				append_buf = NULL;
				append_size = 0;
				// get pathname
				memset(pathname, 0, REQUESTLEN);
				EXIT_IF_EQ(token, NULL, strtok_r(NULL, " ", &saveptr), strtok_r);
				EXIT_IF_NEQ(err, 1, sscanf(token, "%s", pathname), sscanf);
				// get buffer size
				EXIT_IF_EQ(token, NULL, strtok_r(NULL, " ", &saveptr), strtok_r);
				EXIT_IF_NEQ(err, 1, sscanf(token, "%lu", &append_size), sscanf);
				// allocate memory for the buffer
				if (append_size != 0)
				{
					EXIT_IF_EQ(append_buf, NULL, malloc(append_size + 1), malloc);
					memset(append_buf, 0, append_size + 1);
					EXIT_IF_EQ(err, -1, readn((long) fd_ready, append_buf, append_size), readn);
				}
				err = Storage_appendToFile(storage, pathname, append_buf, append_size, &evicted, fd_ready);
				errnocopy = errno;
				free(append_buf);
				// send return value
				memset(request, 0, REQUESTLEN);
				snprintf(request, REQUESTLEN, "%d", err);
				LOG_EVENT("[%d] appendToFile %s : %d -> %lu.\n\tVictims : %lu.\n", (int) pthread_self(), pathname,
							err, append_size, LinkedList_GetNumberOfElements(evicted));
				EXIT_IF_EQ(tmp_err, -1, writen((long) fd_ready, (void*) request, strlen(request) + 1), writen);
				switch (err)
				{
					case OP_SUCCESS:
						break;

					case OP_FAILURE:
						memset(request, 0, REQUESTLEN);
						snprintf(request, REQUESTLEN, "%d", errnocopy);
						EXIT_IF_EQ(err, -1, writen((long) fd_ready, (void*) request, ERRNOLEN), writen);
						break;

					case OP_FATAL:
						memset(request, 0, REQUESTLEN);
						snprintf(request, REQUESTLEN, "%d", errnocopy);
						EXIT_IF_EQ(err, -1, writen((long) fd_ready, (void*) request, ERRNOLEN), writen);
						break;
				}
				// send number of victims
				memset(msg_size, 0, SIZELEN);
				snprintf(msg_size, SIZELEN, "%lu", LinkedList_GetNumberOfElements(evicted));
				EXIT_IF_EQ(tmp_err, -1, writen((long) fd_ready, (void*) msg_size, SIZELEN), writen);
				// send victims if any
				while (LinkedList_GetNumberOfElements(evicted) != 0)
				{
					errno = 0;
					evicted_file_size = LinkedList_PopFront(evicted, &evicted_file_name, (void**) &evicted_file_content);
					if (evicted_file_size == 0 && errno == ENOMEM) exit(1);
					memset(request, 0, REQUESTLEN);
					snprintf(request, REQUESTLEN, "%s", evicted_file_name); // should error handle this
					// send victim's name
					EXIT_IF_EQ(tmp_err, -1, writen((long) fd_ready, (void*) request, REQUESTLEN), writen);
					LOG_EVENT("\tVictim name: %s.\n", evicted_file_name);
					// send victim's contents size
					memset(msg_size, 0, SIZELEN);
					snprintf(msg_size, SIZELEN, "%lu", evicted_file_size);
					EXIT_IF_EQ(tmp_err, -1, writen((long) fd_ready, (void*) msg_size, SIZELEN), writen);
					// send actual contents
					EXIT_IF_EQ(tmp_err, -1, writen((long) fd_ready, (void*) evicted_file_content, evicted_file_size), writen);
					free(evicted_file_name); evicted_file_name = NULL;
					free(evicted_file_content); evicted_file_content = NULL;
				}
				LinkedList_Free(evicted); evicted = NULL;
				if (err == OP_FATAL) exit(1);
				REQUEST_DONE;
				break;

			case READ_N:
				read_files = NULL;
				N = 0;
				// get N
				EXIT_IF_EQ(token, NULL, strtok_r(NULL, " ", &saveptr), strtok_r);
				EXIT_IF_NEQ(err, 1, sscanf(token, "%lu", &N), sscanf);
				err = Storage_readNFiles(storage, &read_files, N, fd_ready);
				errnocopy = errno;
				// send return value
				memset(request, 0, REQUESTLEN);
				snprintf(request, REQUESTLEN, "%d", err);
				EXIT_IF_EQ(tmp_err, -1, writen((long) fd_ready, (void*) request, strlen(request) + 1), writen);
				switch (err)
				{
					case OP_SUCCESS:
						break;

					case OP_FAILURE:
						memset(request, 0, REQUESTLEN);
						snprintf(request, REQUESTLEN, "%d", errnocopy);
						EXIT_IF_EQ(err, -1, writen((long) fd_ready, (void*) request, ERRNOLEN), writen);
						break;

					case OP_FATAL:
						memset(request, 0, REQUESTLEN);
						snprintf(request, REQUESTLEN, "%d", errnocopy);
						EXIT_IF_EQ(err, -1, writen((long) fd_ready, (void*) request, ERRNOLEN), writen);
						break;
				}
				// send number of successfully read files
				memset(msg_size, 0, SIZELEN);
				snprintf(msg_size, SIZELEN, "%lu", LinkedList_GetNumberOfElements(read_files));
				EXIT_IF_EQ(tmp_err, -1, writen((long) fd_ready, (void*) msg_size, SIZELEN), writen);
				// send read files if any
				while (LinkedList_GetNumberOfElements(read_files) != 0)
				{
					errno = 0;
					read_file_size = LinkedList_PopFront(read_files, &read_file_name,
								(void**) &read_file_content);
					if (read_file_size == 0 && errno == ENOMEM) exit(1);
					tot_read_size += read_file_size;
					memset(request, 0, REQUESTLEN);
					snprintf(request, REQUESTLEN, "%s", read_file_name); // should error handle this
					// send file's name
					EXIT_IF_EQ(tmp_err, -1, writen((long) fd_ready, (void*) request, REQUESTLEN), writen);
					// send file's contents size
					memset(msg_size, 0, SIZELEN);
					snprintf(msg_size, SIZELEN, "%lu", read_file_size);
					EXIT_IF_EQ(tmp_err, -1, writen((long) fd_ready, (void*) msg_size, SIZELEN), writen);
					// send actual contents
					EXIT_IF_EQ(tmp_err, -1, writen((long) fd_ready, (void*)
								read_file_content, read_file_size), writen);
					free(read_file_name); read_file_name = NULL;
					free(read_file_content); read_file_content = NULL;
				}
				LinkedList_Free(read_files); read_files = NULL;
				LOG_EVENT("[%d] readNFiles %lu : %d -> %lu.\n", (int) pthread_self(), N, err, tot_read_size);
				if (err == OP_FATAL) exit(1);
				REQUEST_DONE;
				break;

			case LOCK:
				// get pathname
				memset(pathname, 0, REQUESTLEN);
				EXIT_IF_EQ(token, NULL, strtok_r(NULL, " ", &saveptr), strtok_r);
				EXIT_IF_NEQ(err, 1, sscanf(token, "%s", pathname), sscanf);
				err = Storage_lockFile(storage, pathname, fd_ready);
				errnocopy = errno;
				// send return value
				memset(request, 0, REQUESTLEN);
				snprintf(request, REQUESTLEN, "%d", err);
				LOG_EVENT("[%d] lockFile %s %d : %d.\n", (int) pthread_self(), pathname, flags, err);
				EXIT_IF_EQ(tmp_err, -1, writen((long) fd_ready, (void*) request,
							strlen(request) + 1), writen);
				switch (err)
				{
					case OP_SUCCESS:
						break;

					case OP_FAILURE:
						memset(request, 0, REQUESTLEN);
						snprintf(request, REQUESTLEN, "%d", errnocopy);
						EXIT_IF_EQ(err, -1, writen((long) fd_ready, (void*) request,
									ERRNOLEN), writen);
						break;

					case OP_FATAL:
						memset(request, 0, REQUESTLEN);
						snprintf(request, REQUESTLEN, "%d", errnocopy);
						EXIT_IF_EQ(err, -1, writen((long) fd_ready, (void*) request,
									ERRNOLEN), writen);
						exit(1);
				}
				REQUEST_DONE;
				break;

			case UNLOCK:
				// get pathname
				memset(pathname, 0, REQUESTLEN);
				EXIT_IF_EQ(token, NULL, strtok_r(NULL, " ", &saveptr), strtok_r);
				EXIT_IF_NEQ(err, 1, sscanf(token, "%s", pathname), sscanf);
				err = Storage_unlockFile(storage, pathname, fd_ready);
				errnocopy = errno;
				// send return value
				memset(request, 0, REQUESTLEN);
				snprintf(request, REQUESTLEN, "%d", err);
				LOG_EVENT("[%d] unlockFile %s %d : %d.\n", (int) pthread_self(), pathname, flags, err);
				EXIT_IF_EQ(tmp_err, -1, writen((long) fd_ready, (void*) request,
							strlen(request) + 1), writen);
				switch (err)
				{
					case OP_SUCCESS:
						break;

					case OP_FAILURE:
						memset(request, 0, REQUESTLEN);
						snprintf(request, REQUESTLEN, "%d", errnocopy);
						EXIT_IF_EQ(err, -1, writen((long) fd_ready, (void*) request,
									ERRNOLEN), writen);
						break;

					case OP_FATAL:
						memset(request, 0, REQUESTLEN);
						snprintf(request, REQUESTLEN, "%d", errnocopy);
						EXIT_IF_EQ(err, -1, writen((long) fd_ready, (void*) request,
									ERRNOLEN), writen);
						exit(1);
				}
				REQUEST_DONE;
				break;

			case REMOVE:
				// get pathname
				memset(pathname, 0, REQUESTLEN);
				EXIT_IF_EQ(token, NULL, strtok_r(NULL, " ", &saveptr), strtok_r);
				EXIT_IF_NEQ(err, 1, sscanf(token, "%s", pathname), sscanf);
				err = Storage_removeFile(storage, pathname, fd_ready);
				errnocopy = errno;
				// send return value
				memset(request, 0, REQUESTLEN);
				snprintf(request, REQUESTLEN, "%d", err);
				LOG_EVENT("[%d] removeFile %s : %d.\n", (int) pthread_self(), pathname, err);
				EXIT_IF_EQ(tmp_err, -1, writen((long) fd_ready, (void*) request,
							strlen(request) + 1), writen);
				switch (err)
				{
					case OP_SUCCESS:
						break;

					case OP_FAILURE:
						memset(request, 0, REQUESTLEN);
						snprintf(request, REQUESTLEN, "%d", errnocopy);
						EXIT_IF_EQ(err, -1, writen((long) fd_ready, (void*) request,
									ERRNOLEN), writen);
						break;

					case OP_FATAL:
						memset(request, 0, REQUESTLEN);
						snprintf(request, REQUESTLEN, "%d", errnocopy);
						EXIT_IF_EQ(err, -1, writen((long) fd_ready, (void*) request,
									ERRNOLEN), writen);
						exit(1);
				}
				REQUEST_DONE;
				break;

			case TERMINATE:
				memset(pipe_buffer, 0, PIPEBUFFERLEN);
				snprintf(pipe_buffer, PIPEBUFFERLEN, "%d", TERMINATE_WORKER);
				EXIT_IF_EQ(err, -1, writen((long) pipe_output_channel, (void*) pipe_buffer, PIPEBUFFERLEN), writen);
				LOG_EVENT("Client left %d.\n", fd_ready);
				break;
		}
		free(fd_ready_string);
	}
	free(request);
	return NULL;
}