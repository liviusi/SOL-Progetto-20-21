/**
 * @brief Client file.
 * @author Giacomo Trapani.
*/

#define _DEFAULT_SOURCE // usleep

#include <dirent.h>
#include <linux/limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <linked_list.h>
#include <server_defines.h>
#include <server_interface.h>
#include <utilities.h>
#include <wrappers.h>

#define COMMANDLEN 2
#define MAX_NAME 128
#define BUFFERLEN 512
#define ARGUMENTLEN 2048

// Used to check whether character corresponds to a valid command
#define VALID_COMMAND(character) \
	(character == 'h' || character == 'f' || character == 'w' || \
	character == 'W' || character == 'd' || character == 'D' || \
	character == 'r' || character == 'R' || character == 't' || \
	character == 'l' || character == 'u' || character == 'c' || \
	character == 'p')


#define H_MESSAGE \
"Client accepts these command line arguments:\n"\
"-h : prints this message.\n"\
"-f <filename> : connects to given socket.\n"\
"-w <dirname>[,n=0] : sends to server up to n files in given directory and its subdirectories.\n"\
"-W <file1>[,file2] : sends to server given files.\n"\
"-D <dirname> : specifies the folder evicted files are to be sent to.\n"\
"-r <file1>[,file2] : reads given files from server.\n"\
"-R [n=0] : reads at least n files from server (if unspecified, it reads every file).\n"\
"-d <dirname> : specifies the folder read files are to be stored in.\n"\
"-t <time> : specifies time to wait between requests.\n"\
"-l <file1>[,file2] : requests lock over given files.\n"\
"-u <file1>[,file2] : releases lock over given files.\n"\
"-c <file1>[,file2] : requests server to remove given files.\n"\
"-p : enables output on stdout.\n"

/**
 * Returns true if and only if string is only made up of dots.
*/
static bool
dots_only(const char dir[]);

/**
 * @brief Returns current working directory as a dynamically allocated buffer.
 * @returns Initialized buffer on success, NULL on failure.
 * @exception The function may fail and set "errno" for any of the errors specified for routines "malloc", "getcwd".
*/
static char*
cwd();

/**
 * @brief Visits given directory and all its subdirectories thus initializing given list's key to the absolute path
 * of every regular file.
 * @returns 0 on success, -1 on failure.
 * @param dirname cannot be NULL.
 * @param files cannot be NULL.
 * @exception It sets "errno" to "EINVAL" if any param is not valid. The function may also fail and set "errno"
 * for any of the errors specified for routines "opendir", "readdir", "malloc", "cwd", "chdir", "LinkedList_PushBack".
 * 
*/
static int
list_files(const char dirname[], linked_list_t* list);

/**
 * @brief Validates commands and arguments. It also sets h_set and sockname.
 * @param commands cannot be NULL.
 * @param arguments cannot be NULL.
 * @exception It sets errno to EINVAL if any param is not valid of given input is not a valid one.
*/
static int
validate(const char** commands, const char** arguments, int len);

/**
 * Used to free allocated resources on fatal errors.
*/
void
cleanup_func();

/**
 * As the flag defined in the server interface "exit_on_fatal_errors" will be toggled on
 * a cleanup function is to be called at exit; therefore, dynamically allocated resources
 * are to be declared at a global level so that they can be freed (at exit).
*/

bool cleaned_up = false; // toggled on when program reaches cleanup or failure label
bool connected = false; // toggled on when client is connected
char** commands = NULL; // commands to be executed
char** arguments = NULL; // arguments of the commands to be executed
linked_list_t* R_files = NULL; // list of files to be sent by a -w command
int len = 0; // length of commands and arguments buffers
bool h_set = false; // toggled on when -h flag is passed
char sockname[MAXPATH]; // name of the socket to connect to
char* filename = NULL; // used to denote a filename
char* read_contents = NULL; // used to store read content



int
main(int argc, char* argv[])
{
	if (argc == 1)
	{
		fprintf(stderr, "No arguments were specified.\n");
		return 1;
	}

	// -------------
	// DECLARATIONS
	// -------------
	len = argc - 1;
	char flag; // used to denote a flag
	int msec = 1000; // default value
	int msec_sleeping = 0; // milliseconds client shall sleep between requests
	char* tmp; // used when parsing through commands' arguments
	char* token; // used when parsing through commands' arguments
	char* saveptr; // used when parsing through commands' arguments
	int open_flags = 0; // used to denote flags for file opening operation
	struct timespec abstime = { .tv_nsec = 0, .tv_sec = time(0) + 10 }; // default value
	size_t read_size = 0; // used to store read content size
	char* cwd_copy = NULL; // copy of current working directory
	int upto = 0; // used to denote N value in readNFiles operation
	char filepath[PATH_MAX]; // used to denote path to a file
	int err; // used to handle functions' output values
	int i = 0; // index used in loops 

	// ---------------------
	// INITIALIZE INTERNALS
	// ---------------------
	commands = (char**) malloc(sizeof(char*) * (argc - 1));
	if (!commands)
	{
		perror("malloc");
		goto failure;
	}
	for (i = 0; i < argc - 1; i++)
	{
		commands[i] = (char*) malloc(sizeof(char) * COMMANDLEN);
		if (!commands[i])
		{
			perror("malloc");
			goto failure;
		}
		memset(commands[i], 0, COMMANDLEN);
	}
	arguments = (char**) malloc(sizeof(char*) * (argc - 1));
	if (!arguments)
	{
		perror("malloc");
		goto failure;
	}
	for (i = 0; i < argc - 1; i++)
	{
		arguments[i] = (char*) malloc(sizeof(char) * ARGUMENTLEN);
		if (!arguments[i])
		{
			perror("malloc");
			goto failure;
		}
		memset(arguments[i], 0, ARGUMENTLEN);
	}

	// ---------------
	// VALIDATE INPUT
	// ---------------
	// parse argv into commands to be executed
	for (int i = 1; i < argc; i++)
	{
		if (argv[i][0] == '-')
		{
			int len = strlen(argv[i]);
			int j = 0;
			while (j < len)
			{
				if (argv[i][j] == '-')
				{
					j++;
					continue;
				}
				else
				{
					if (j == len-1 && VALID_COMMAND(argv[i][j]))
						snprintf(commands[i-1], COMMANDLEN, "%s", argv[i] + j);
					break;
				}
			}
		}
	}
	if (commands[0][0] == '\0') goto cleanup;
	// parse argv into commands' arguments
	for (int i = 1; i < argc; i++)
	{
		if (commands[i-1][0] != '\0') continue;
		snprintf(arguments[i-2], ARGUMENTLEN, "%s", argv[i]);
	}
	print_enabled = false;
	exit_on_fatal_errors = true;
	err = validate((const char**) commands, (const char**) arguments, argc-1);
	if (err != 0)
	{
		fprintf(stderr, "Given input is not a valid sequence.\n");
		goto cleanup;
	}

	// -----------------
	// EXECUTE COMMANDS
	// -----------------
	atexit(cleanup_func);
	if (h_set)
	{
		fprintf(stdout, H_MESSAGE);
		goto cleanup;
	}
	for (int i = 0; i < argc - 1; i++)
	{
		if (commands[i][0] == '\0') continue;
		else flag = commands[i][0];
		switch (flag)
		{
			case 'f':
				// execute openConnection
				if (openConnection(sockname, msec, abstime) == 0)
					connected = true;
				break;

			case 'w':
				// write directory
				// get dirname from arguments
				tmp = arguments[i];
				R_files = LinkedList_Init(NULL);
				if (!R_files)
				{
					perror("LinkedList_Init");
					goto cleanup;
				}
				cwd_copy = cwd();
				if (!cwd_copy)
				{
					perror("cwd");
					goto cleanup;
				}
				// check whether a limit of files to be sent has been specified
				if (strrchr(tmp, ',') == NULL) // no limit
				{
					if (list_files(arguments[i], R_files) == -1)
					{
						if (errno == ENOMEM)
						{
							perror("list_files");
							goto cleanup;
						}
						else break;
					}
					if (chdir(cwd_copy) == -1)
					{
						perror("chdir");
						goto cleanup;
					}
					free(cwd_copy); cwd_copy = NULL;
					while (LinkedList_GetNumberOfElements(R_files) != 0)
					{
						errno = 0;
						if (LinkedList_PopFront(R_files, &filename, NULL) == 0 && errno == ENOMEM)
						{
							perror("LinkedList_PopFront");
							goto cleanup;
						}
						SET_FLAG(open_flags, O_CREATE);
						SET_FLAG(open_flags, O_LOCK);
						openFile(filename, open_flags);
						RESET_MASK(open_flags);
						if (i + 2 < argc -1)
						{
							if (commands[i+2][0] == 'D')
								writeFile(filename, arguments[i+2]);
						}
						else
							writeFile(filename, NULL);
						unlockFile(filename);
						closeFile(filename);
						usleep(1000 * msec_sleeping);
						free(filename); filename = NULL;
					}
					LinkedList_Free(R_files); R_files = NULL;
					break;

				}
				else
				{
					token = strtok_r(tmp, ",", &saveptr);
					errno = 0;
					if (list_files(token, R_files) == -1)
					{
						if (errno == ENOMEM)
						{
							perror("list_files");
							goto cleanup;
						}
						else break;
					}
					if (chdir(cwd_copy) == -1)
					{
						perror("chdir");
						goto cleanup;
					}
					free(cwd_copy); cwd_copy = NULL;
					token = strtok_r(NULL, ",", &saveptr);
					if (sscanf(token, "%d", &upto) != 1)
					{
						perror("sscanf");
						goto cleanup;
					}
					while (upto > 0)
					{
						if (LinkedList_GetNumberOfElements(R_files) == 0) break;
						errno = 0;
						if (LinkedList_PopFront(R_files, &filename, NULL) == 0 && errno == ENOMEM)
						{
							perror("LinkedList_PopFront");
							goto cleanup;
						}
						SET_FLAG(open_flags, O_CREATE);
						SET_FLAG(open_flags, O_LOCK);
						openFile(filename, open_flags);
						RESET_MASK(open_flags);
						if (i + 2 < argc -1)
						{
							if (commands[i+2][0] == 'D')
								writeFile(filename, arguments[i+2]);
						}
						else
							writeFile(filename, NULL);
						unlockFile(filename);
						closeFile(filename);
						usleep(1000 * msec_sleeping);
						free(filename); filename = NULL;
						upto--;
					}
					LinkedList_Free(R_files); R_files = NULL;
					break;
				}
				break;

			case 'W':
				// write files
				tmp = arguments[i];
				// check whether multiple files have been specified
				if (strchr(tmp, ',') == NULL) // single file
				{
					SET_FLAG(open_flags, O_CREATE);
					SET_FLAG(open_flags, O_LOCK);
					openFile(tmp, open_flags);
					RESET_MASK(open_flags);
					if (i + 2 < argc -1)
					{
						if (commands[i+2][0] == 'D')
							writeFile(tmp, arguments[i+2]);
					}
					else
						writeFile(tmp, NULL);
					unlockFile(tmp);
					closeFile(tmp);
					usleep(1000 * msec_sleeping);
					break;
				}
				else // multiple files
				{
					token = strtok_r(tmp, ",", &saveptr);
					while (1)
					{
						if (!token) break;
						SET_FLAG(open_flags, O_CREATE);
						SET_FLAG(open_flags, O_LOCK);
						openFile(token, open_flags);
						RESET_MASK(open_flags);
						if (i + 2 < argc - 1 && commands[i+2][0] == 'D')
							writeFile(token, arguments[i+2]);
						else
							writeFile(token, NULL);
						unlockFile(token);
						closeFile(token);
						usleep(msec_sleeping * 1000);
						token = strtok_r(NULL, ",", &saveptr);
					}
				}
				break;

			case 'r':
				// read files
				tmp = arguments[i];
				// check whether multiple files have been specified
				if (strchr(tmp, ',') == NULL) // single file
				{
					RESET_MASK(open_flags);
					openFile(tmp, open_flags);
					readFile(tmp, (void** )&read_contents, &read_size);
					closeFile(tmp);
					if (i + 2 < argc - 1 && commands[i+2][0] == 'd')
					{
						// prepend dirname to filename
						memset(filepath, 0, PATH_MAX);
						err = snprintf(filepath, PATH_MAX, "%s/%s", arguments[i+2], tmp);
						if (err <= 0 || err > PATH_MAX)
						{
							perror("snprintf");
							goto cleanup;
						}
						if (savefile(filepath, read_contents) == -1)
						{
							perror("savefile");
							goto cleanup;
						}
					}
					free(read_contents); read_contents = NULL;
					usleep(1000 * msec_sleeping);
					break;
				}
				else // multiple files
				{
					token = strtok_r(tmp, ",", &saveptr);
					while (1)
					{
						if (!token) break;
						RESET_MASK(open_flags);
						openFile(token, open_flags);
						readFile(token, (void**) &read_contents, &read_size);
						closeFile(token);
						if (i + 2 < argc - 1 && commands[i+2][0] == 'd')
						{
							// prepend dirname to filename
							memset(filepath, 0, PATH_MAX);
							err = snprintf(filepath, PATH_MAX, "%s/%s", token, arguments[i+2]);
							if (err <= 0 || err > PATH_MAX)
							{
								perror("snprintf");
								goto cleanup;
							}
							if (savefile(filepath, read_contents) == -1)
							{
								perror("savefile");
								goto cleanup;
							}

						}
						free(read_contents); read_contents = NULL;
						usleep(1000 * msec_sleeping);
						token = strtok_r(NULL, ",", &saveptr);
						
					}
				}
				break;
			
			case 'R':
				// read up to N files
				tmp = arguments[i];
				// check whether N has been specified
				if (tmp[0] != '\0')
					sscanf(arguments[i], "%d", &upto);
				else
					upto = 0;
				if (i + 2 < argc - 1 && commands[i+2][0] == 'd')
					readNFiles(upto, arguments[i+2]);
				else
					readNFiles(upto, NULL);
				usleep(1000 * msec_sleeping);
				break;

			case 't':
				// set waiting time
				sscanf(arguments[i], "%d", &msec_sleeping); // cannot fail
				break;

			case 'l':
				tmp = arguments[i];
				RESET_MASK(open_flags);
				// check whether multiple files have been specified
				if (strchr(tmp, ',') == NULL) // single file
				{
					openFile(tmp, open_flags);
					lockFile(tmp);
					closeFile(tmp);
					usleep(1000 * msec_sleeping);
				}
				else // multiple files
				{
					token = strtok_r(tmp, ",", &saveptr);
					while (1)
					{
						if (!token) break;
						openFile(token, open_flags);
						lockFile(token);
						closeFile(token);
						usleep(1000 * msec_sleeping);
						token = strtok_r(NULL, ",", &saveptr);
					}
				}
				break;

			case 'u':
				tmp = arguments[i];
				RESET_MASK(open_flags);
				// check whether multiple files have been specified
				if (strchr(tmp, ',') == NULL) // single file
				{
					openFile(tmp, open_flags);
					unlockFile(tmp);
					closeFile(tmp);
					usleep(1000 * msec_sleeping);
				}
				else // multiple files
				{
					token = strtok_r(tmp, ",", &saveptr);
					while (1)
					{
						if (!token) break;
						openFile(token, open_flags);
						unlockFile(token);
						closeFile(token);
						token = strtok_r(NULL, ",", &saveptr);
						usleep(1000 * msec_sleeping);
					}
				}
				break;
			case 'c':
				tmp = arguments[i];
				RESET_MASK(open_flags);
				// check whether multiple files have been specified
				if (strchr(tmp, ',') == NULL) // single file
				{
					openFile(tmp, open_flags);
					removeFile(tmp);
					usleep(1000 * msec_sleeping);
				}
				else // multiple files
				{
					token = strtok_r(tmp, ",", &saveptr);
					while (1)
					{
						if (!token) break;
						openFile(token, open_flags);
						removeFile(token);
						closeFile(token);
						token = strtok_r(NULL, ",", &saveptr);
						usleep(1000 * msec_sleeping);
					}
				}
				break;

			default:
				break;
		}
	}
	goto cleanup;



	cleanup:
		if (connected) closeConnection(sockname);
		for (int i = 0; i < argc - 1; i++)
		{
			free(commands[i]);
			free(arguments[i]);
		}
		free(commands);
		free(arguments);
		LinkedList_Free(R_files);
		cleaned_up = true;
		return 0;

	failure:
		if (arguments)
		{
			int j = 0;
			while (j != i)
			{
				free(commands[j]);
				j++;
			}
			i = argc - 1; // every command has been initialized
		}
		free(arguments);
		if (commands)
		{
			int j = 0;
			while (j != i)
			{
				free(commands[j]);
				j++;
			}
		}
		free(commands);
		cleaned_up = true;
		return 1;
}

static bool
dots_only(const char dir[])
{
	if (!dir) return false;
	int l = strlen(dir);
	if (l > 0 && dir[l-1] == '.')
		return true;
	return false;
}

static char*
cwd()
{
	char* buf = malloc(MAX_NAME * sizeof(char));
	if (!buf) return NULL;
	return getcwd(buf, MAX_NAME);
}

static int
list_files(const char dirname[], linked_list_t* files)
{
	if (!dirname || !files)
	{
		errno = EINVAL;
		return -1;
	}
	// changing directory in order to run opendir(".")
	if (chdir(dirname) == -1) return 0;
	DIR* dir;
	if ((dir=opendir(".")) == NULL) return -1;
	else
	{
		struct dirent *file;
		while((errno = 0, file = readdir(dir)) != NULL)
		{
			if (file->d_type == DT_REG) // if regular file
			{
				char* buf = cwd();
				if (!buf) return -1;
				char* path = (char*) malloc(sizeof(char) * BUFFERLEN);
				if (!path) return -1;
				memset(path, 0, BUFFERLEN);
				snprintf(path, BUFFERLEN, "%s/%s", buf, file->d_name);
				if (LinkedList_PushBack(files, path, strlen(path) + 1, NULL, 0) == -1) return -1;
				free(path);
				free(buf);
			}
			// do not loop over the same folder or its parents
			else if (!dots_only(file->d_name))
			{
				if (list_files(file->d_name, files) != 0)
				{
					if (chdir("..") == -1) return -1;
				}
			}
		}
		if (errno != 0) return -1;
		closedir(dir);
	}
	return 1;
}

static int
validate(const char** commands, const char** arguments, int len)
{
	if (!commands || !arguments || len <= 0)
	{
		errno = EINVAL;
		return 1;
	}
	if (commands[0][0] == '\0')
	{
		errno = EINVAL;
		return 1;
	}
	for (int i = 0; i < len; i++)
	{
		// checking for syntax errors
		if (arguments[i][0] != '\0' && commands[i][0] == '\0')
		{
			errno = EINVAL;
			return 1;
		}
		if (commands[i][0] == '\0') continue;
		if (commands[i][0] == 'h')
		{
			// -h takes no arguments
			if (arguments[i][0] != '\0')
			{
				errno = EINVAL;
				return 1;
			}
			// h can be specified at most once
			for (int j = i+1; j < len; j++)
			{
				if (commands[j][0] == 'h')
				{
					errno = EINVAL;
					return 1;
				}
			}
			h_set = true;
			continue;
		}
		if (commands[i][0] == 'f')
		{
			// -f takes an argument
			if (arguments[i][0] == '\0')
			{
				errno = EINVAL;
				return 1;
			}
			// -f can be specified at most once
			for (int j = i+1; j < len; j++)
			{
				if (commands[j][0] == 'f')
				{
					errno = EINVAL;
					return 1;
				}
			}
			memset(sockname, 0, MAXPATH);
			snprintf(sockname, MAXPATH, "%s", arguments[i]);
			continue;
		}
		if (commands[i][0] == 'p')
		{
			// -p takes no argument
			if (arguments[i][0] != '\0')
			{
				errno = EINVAL;
				return 1;
			}
			// -p can be specified at most once
			for (int j = i+1; j < len; j++)
			{
				if (commands[j][0] == 'p')
				{
					errno = EINVAL;
					return 1;
				}
			}
			print_enabled = true;
			continue;
		}
		// dangling commas and invalid arguments are not allowed
		if (commands[i][0] == 'w')
		{
			if (arguments[i][0] == '\0')
			{
				errno = EINVAL;
				return 1;
			}
			if (strrchr(arguments[i], ',') == NULL) continue;
			else
			{
				char* copy = (char*) malloc(sizeof(char) * (strlen(arguments[i]) + 1));
				if (!copy) return 1; // errno has already been set
				strcpy(copy, arguments[i]);
				char* tmp = copy;
				char* saveptr;
				char* token = strtok_r(copy, ",", &saveptr);
				// need to check what comes after the comma
				token = strtok_r(NULL, ",", &saveptr);
				if (!token)
				{
					free(tmp);
					return 1;
				}
				int value;
				// optional argument needs to be a number
				if (sscanf(token, "%d", &value) != 1)
				{
					free(tmp);
					errno = EINVAL;
					return 1;
				}
				token = strtok_r(NULL, ",", &saveptr);
				if (token)
				{
					free(tmp);
					errno = EINVAL;
					return 1;
				}
				free(tmp);
				continue;
			}
		}
		// dangling commas are not allowed
		if (commands[i][0] == 'W' || commands[i][0] == 'r' || commands[i][0] == 'l' ||
					commands[i][0] == 'c' || commands[i][0] == 'u')
		{
			if (arguments[i][0] == '\0')
			{
				errno = EINVAL;
				return 1;
			}
			if (strrchr(arguments[i], ',') == NULL) continue;
			if (arguments[i][strlen(arguments[i]) - 1] == ',')
			{
				errno = EINVAL;
				return 1;
			}
		}
		// an optional numeric number is expected
		if (commands[i][0] == 'R')
		{
			if (arguments[i][0] == '\0') continue;
			int tmp;
			if (sscanf(arguments[i], "%d", &tmp) != 1)
			{
				errno = EINVAL;
				return 1;
			}
			continue;
		}
		// a numeric argument is expected
		if (commands[i][0] == 't')
		{
			if (arguments[i][0] == '\0')
			{
				errno = EINVAL;
				return 1;
			}
			int tmp;
			if (sscanf(arguments[i], "%d", &tmp) != 1)
			{
				errno = EINVAL;
				return 1;
			}
			continue;
		}

		// checking whether the commands given
		// are a valid input sequence

		if (commands[i][0] == 'd') // if files are to be saved
		{
			// there must be a reading operation before
			if (i == 0)
			{
				errno = EINVAL;
				return 1;
			}
			if (arguments[i][0] == '\0')
			{
				errno = EINVAL;
				return 1;
			}
			if (commands[i-2][0] != 'r' && commands[i-2][0] != 'R' && commands[i-1][0] != 'R')
			{
				errno = EINVAL;
				return 1;
			}
			continue;
		}
		if (commands[i][0] == 'D') // if files are to be saved
		{
			// there must be a writing operation before
			if (i == 0)
			{
				errno = EINVAL;
				return 1;
			}
			if (arguments[i][0] == '\0')
			{
				errno = EINVAL;
				return 1;
			}
			if (commands[i-2][0] != 'w' && commands[i-2][0] != 'W')
			{
				errno = EINVAL;
				return 1;
			}
			continue;
		}
	}
	return 0;
}

void
cleanup_func()
{
	if (cleaned_up) return;
	if (connected) closeConnection(sockname);
	for (int i = 0; i < len; i++)
	{
		free(commands[i]);
		free(arguments[i]);
	}
	free(commands);
	free(arguments);
	LinkedList_Free(R_files);
	free(filename);
	free(read_contents);
	return;
}