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
#include <pthread.h>
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
#define MAXTASKS 4096

#define TERMINATE_WORKER 0 // used to send a termination message
#define CLIENT_LEFT "0"

#define NEXT_ITERATION \
{ \
	free(fd_ready_string); \
	continue; \
}

#define REQUEST_DONE \
{ \
	memset(pipe_buffer, 0, PIPEBUFFERLEN); \
	snprintf(pipe_buffer, PIPEBUFFERLEN, "%d", fd_ready); \
	EXIT_IF_EQ(err, -1, writen((long) pipe_output_channel, (void*) pipe_buffer, PIPEBUFFERLEN), writen); \
	break; \
}

#define LOG_EVENT(...) \
do \
{ \
	if (pthread_mutex_lock(&log_mutex) != 0) { perror("pthread_mutex_lock"); exit(1); } \
	fprintf(log_file, __VA_ARGS__); \
	if (pthread_mutex_unlock(&log_mutex) != 0) { perror("pthread_mutex_unlock"); exit(1); } \
} while(0);

volatile sig_atomic_t terminate = 0;
volatile sig_atomic_t no_more_clients = 0;

pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

static void* worker_routine(void*);
void signal_handler(int);

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
	char* log_name = NULL;
	FILE* log_file = NULL;

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

	// initialize log file
	EXIT_IF_EQ(err, 0, (int) ServerConfig_GetLogFilePath(config, &log_name), ServerConfig_GetLogFilePath);
	mode_t oldmask = umask(033);
	EXIT_IF_EQ(log_file, NULL, fopen(log_name, "w+"), fopen);
	umask(oldmask);
	free(log_name);

	// initialize workers' arguments
	EXIT_IF_EQ(workers_args, NULL, (struct workers_args*) malloc(sizeof(struct workers_args)), malloc);
	workers_args->storage = storage;
	workers_args->tasks = tasks;
	workers_args->pipe_output_channel = pipe_worker2manager[1];
	workers_args->log_file = log_file;

	// initialize workers pool
	EXIT_IF_EQ(workers_pool_size, 0, ServerConfig_GetWorkersNo(config), ServerConfig_GetWorkersNo);
	EXIT_IF_EQ(workers, NULL, (pthread_t*) malloc(sizeof(pthread_t) * workers_pool_size), malloc);
	for (size_t i = 0; i < (size_t) workers_pool_size; i++)
		EXIT_IF_EQ(err, -1, pthread_create(&(workers[i]), NULL, &worker_routine, (void*) workers_args), pthread_create);

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
					snprintf(new_task, TASKLEN, "%d", i);
					//FD_CLR(i, &master_read_set);
					if (i == fd_num) fd_num--;
					// push ready file descriptor to task queue for workers
					EXIT_IF_EQ(err, -1, BoundedBuffer_Enqueue(tasks, new_task), BoundedBuffer_Enqueue);
				}
			}
		}
	}

	return 0;


	cleanup:
		snprintf(new_task, TASKLEN, "%d", TERMINATE_WORKER);
		for (size_t i = 0; i < (size_t) workers_pool_size; i++)
			EXIT_IF_NEQ(err, 0, BoundedBuffer_Enqueue(tasks, new_task), BoundedBuffer_Enqueue);
		for (size_t i = 0; i < (size_t) workers_pool_size; i++)
			pthread_join(workers[i], NULL);
		ServerConfig_Free(config);
		Storage_Print(storage);
		LOG_EVENT("Maximum size reached : %5f.\n", Storage_GetReachedSize(storage) * MBYTE);
		LOG_EVENT("Maximum file number : %lu.\n", Storage_GetReachedFiles(storage));
		Storage_Free(storage);
		BoundedBuffer_Free(tasks);
		unlink(sockname);
		free(sockname);
		free(workers_args);
		free(workers);
		close(pipe_worker2manager[0]);
		close(pipe_worker2manager[1]);
		fclose(log_file);
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
	linked_list_t* evicted = NULL; // used to store evicted files
	void* read_buf = NULL; // buffer used for reading operation
	size_t read_size = 0; // size of read_buf
	void* append_buf = NULL; // buffer used for append operation
	size_t append_size = 0; // size of append_buf
	int flags = 0; // used to denote flags for operations on storage
	char* evicted_file_name = NULL; // name of evicted file
	char* evicted_file_content = NULL; // content of evicted file
	size_t evicted_file_size = 0; // size of evicted file content
	char pipe_buffer[PIPEBUFFERLEN]; // buffer to be written on pipe
	char msg_size[SIZELEN];
	size_t write_size = 0;
	char* write_contents = NULL;
	linked_list_t* read_files = NULL;
	char* read_file_name = NULL;
	char* read_file_content = NULL;
	size_t read_file_size = 0;
	size_t tot_read_size = 0;
	size_t N = 0;
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