/**
 * @brief
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

#define VALID_COMMAND(character) \
	character == 'h' || character == 'f' || character == 'w' || \
	character == 'W' || character == 'd' || character == 'D' || \
	character == 'r' || character == 'R' || character == 't' || \
	character == 'l' || character == 'u' || character == 'c' || \
	character == 'p'


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
 * @brief Returns true if and only if string is only
 * made up of dots.
*/
static bool
dots_only(const char dir[]);

/**
 * @brief Returns current working directory as a dynamically
 * allocated buffer.
*/
static char*
cwd();

/**
 * @brief Visits directory and all its subdirectories.
*/
static int
list_files(const char dirname[], linked_list_t* list);

/**
 * @brief Validates commands and arguments. It also sets
 * flags.
*/
int
validate(const char** commands, const char** arguments, int len);

bool h_set = false;
char sockname[MAXPATH];

int
main(int argc, char* argv[])
{
	if (argc == 1)
	{
		fprintf(stderr, "No arguments were specified.\n");
		return 1;
	}
	// bool found_h = false;
	// states_t curr_state = EXPECTING_HYPHEN;
	char** commands;
	EXIT_IF_EQ(commands, NULL, (char**) malloc(sizeof(char*) * (argc - 1)), malloc);
	for (int i = 0; i < argc - 1; i++)
	{
		EXIT_IF_EQ(commands[i], NULL, (char*) malloc(sizeof(char) * COMMANDLEN), malloc);
		memset(commands[i], 0, COMMANDLEN);
	}
	char** arguments;
	EXIT_IF_EQ(arguments, NULL, (char**) malloc(sizeof(char*) * (argc - 1)), malloc);
	for (int i = 0; i < argc - 1; i++)
	{
		EXIT_IF_EQ(arguments[i], NULL, (char*) malloc(sizeof(char) * ARGUMENTLEN), malloc);
		memset(arguments[i], 0, ARGUMENTLEN);
	}
	// declarations
	char flag; int msec = 1000;
	int msec_sleeping = 0; // milliseconds client shall sleep between requests
	char* tmp; char* token; char* saveptr;
	linked_list_t* R_files = NULL; char* filename = NULL;
	int open_flags = 0;
	struct timespec abstime = { .tv_nsec = 0, .tv_sec = time(0) + 10 };
	char* read_contents = NULL; size_t read_size = 0;
	char* cwd_copy = NULL;
	int upto = 0;
	char filepath[PATH_MAX];
	bool connected = false;
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
					if (j == len-1 && (VALID_COMMAND(argv[i][j])))
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
	int err = validate((const char**) commands, (const char**) arguments, argc-1);
	if (err != 0)
	{
		fprintf(stderr, "Given input is not a valid sequence.\n");
		goto cleanup;
	}
	// execute commands
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
				if (!R_files) goto cleanup;
				cwd_copy = cwd();
				if (!cwd_copy) goto cleanup;
				
				// check whether a limit of files to be sent has been specified
				if (strrchr(tmp, ',') == NULL) // no limit
				{
					if (list_files(arguments[i], R_files) == -1)
					{
						if (errno == ENOMEM) goto cleanup;
						else break;
					}
					if (chdir(cwd_copy) == -1) goto cleanup;
					free(cwd_copy); cwd_copy = NULL;
					while (LinkedList_GetNumberOfElements(R_files) != 0)
					{
						errno = 0;
						if (LinkedList_PopFront(R_files, &filename, NULL) == 0 && errno == ENOMEM)
							goto cleanup;
						//fprintf(stderr, "filename : %s\n", filename);
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
					if (list_files(token, R_files) == -1)
					{
						if (errno == ENOMEM) goto cleanup;
						else break;
					}
					if (chdir(cwd_copy) == -1) goto cleanup;
					free(cwd_copy); cwd_copy = NULL;
					token = strtok_r(NULL, ",", &saveptr);
					if (sscanf(token, "%d", &upto) != 1) goto cleanup;
					while (upto > 0)
					{
						if (LinkedList_GetNumberOfElements(R_files) == 0) break;
						if (LinkedList_PopFront(R_files, &filename, NULL) == 0 && errno == ENOMEM)
							goto cleanup;
						fprintf(stderr, "filename : %s\n", filename);
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
						snprintf(filepath, PATH_MAX, "%s/%s", arguments[i+2], tmp); // TODO: ERROR HANDLING
						savefile(filepath, read_contents); // TODO: ERROR HANDLING
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
							snprintf(filepath, PATH_MAX, "%s/%s", token, arguments[i+2]); // TODO: ERROR HANDLING
							savefile(filepath, read_contents); // TODO: ERROR HANDLING
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
				sscanf(arguments[i], "%d", &msec_sleeping);
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
		return 0;
}

static bool
dots_only(const char dir[])
{
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

/**
*/
static int
list_files(const char dirname[], linked_list_t* files)
{
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
				if (buf == NULL) return -1;
				char* path = (char*) malloc(sizeof(char) * BUFFERLEN);
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

int
validate(const char** commands, const char** arguments, int len)
{
	if (commands[0][0] == '\0') return 1;
	for (int i = 0; i < len; i++)
	{
		// checking for syntax errors
		if (arguments[i][0] != '\0' && commands[i][0] == '\0') return 1;
		if (commands[i][0] == '\0') continue;
		if (commands[i][0] == 'h')
		{
			// -h takes no arguments
			if (arguments[i][0] != '\0') return 1;
			// h can be specified at most once
			for (int j = i+1; j < len; j++)
				if (commands[j][0] == 'h') return 1;
			h_set = true;
			continue;
		}
		if (commands[i][0] == 'f')
		{
			// -f takes an argument
			if (arguments[i][0] == '\0') return 1;
			// -f can be specified at most once
			for (int j = i+1; j < len; j++)
				if (commands[j][0] == 'f') return 1;
			memset(sockname, 0, MAXPATH);
			snprintf(sockname, MAXPATH, "%s", arguments[i]);
			continue;
		}
		if (commands[i][0] == 'p')
		{
			// -p takes no argument
			if (arguments[i][0] != '\0') return 1;
			// -p can be specified at most once
			for (int j = i+1; j < len; j++)
				if (commands[j][0] == 'p') return 1;
			print_enabled = true;
			continue;
		}
		// dangling commas and invalid arguments are not allowed
		if (commands[i][0] == 'w')
		{
			if (arguments[i][0] == '\0') return 1;
			if (strrchr(arguments[i], ',') == NULL) continue;
			else
			{
				char* copy = (char*) malloc(sizeof(char) * (strlen(arguments[i]) + 1));
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
					return 1;
				}
				token = strtok_r(NULL, ",", &saveptr);
				if (token)
				{
					free(tmp);
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
			if (arguments[i][0] == '\0') return 1;
			if (strrchr(arguments[i], ',') == NULL) continue;
			if (arguments[i][strlen(arguments[i]) - 1] == ',') return 1;
		}
		// an optional numeric number is expected
		if (commands[i][0] == 'R')
		{
			if (arguments[i][0] == '\0') continue;
			int tmp;
			if (sscanf(arguments[i], "%d", &tmp) != 1) return 1;
			continue;
		}
		// a numeric argument is expected
		if (commands[i][0] == 't')
		{
			if (arguments[i][0] == '\0') return 1;
			int tmp;
			if (sscanf(arguments[i], "%d", &tmp) != 1) return 1;
			continue;
		}

		// checking whether the commands given
		// are a valid input sequence

		if (commands[i][0] == 'd') // if files are to be saved
		{
			// there must be a reading operation before
			if (i == 0) return 1;
			if (arguments[i][0] == '\0') return 1;
			if (commands[i-2][0] != 'r' && commands[i-2][0] != 'R' &&
						commands[i-1][0] != 'R')
				return 1;
			continue;
		}
		if (commands[i][0] == 'D') // if files are to be saved
		{
			// there must be a writing operation before
			if (i == 0) return 1;
			if (arguments[i][0] == '\0') return 1;
			if (commands[i-2][0] != 'w' && commands[i-2][0] != 'W')
				return 1;
			continue;
		}
	}
	return 0;
}