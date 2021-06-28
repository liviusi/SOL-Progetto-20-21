/**
 * @brief
 * @author Giacomo Trapani.
*/

#define _DEFAULT_SOURCE

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <server_defines.h>
#include <server_interface.h>
#include <wrappers.h>

#define COMMANDLEN 2
#define ARGUMENTLEN 2048

#define VALID_COMMAND(character) \
	character == 'h' || character == 'f' || character == 'w' || \
	character == 'W' || character == 'd' || character == 'D' || \
	character == 'r' || character == 'R' || character == 't' || \
	character == 'l' || character == 'u' || character == 'c' || \
	character == 'p'

/**
 * @brief Validates commands and arguments. It also sets
 * flags.
*/
int
validate(char** commands, char** arguments, int len);

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
	int err = validate(commands, arguments, argc-1);
	if (err != 0)
	{
		fprintf(stderr, "Given input is not a valid sequence.\n");
		goto cleanup;
	}
	char flag; int msec = 1000;
	struct timespec abstime = { .tv_nsec = 0, .tv_sec = time(0) + 10 };
	// execute commands
	for (int i = 0; i < argc - 1; i++)
	{
		if (commands[i][0] == '\0') continue;
		else flag = commands[i][0];
		switch (flag)
		{
			case 'f':
				// execute openConnection
				openConnection(sockname, msec, abstime);
				break;
			default:
				break;
		}
	}
	goto cleanup;

cleanup:
	for (int i = 0; i < argc - 1; i++)
	{
		fprintf(stdout, "commands[%d] : %s", i, commands[i]);
		fprintf(stdout, "\targuments[%d] : %s\n", i, arguments[i]);
		free(commands[i]);
		free(arguments[i]);
	}
	free(commands);
	free(arguments);
	return 0;
}

int
validate(char** commands, char** arguments, int len)
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
		if (commands[i][0] == 'w' || commands[i][0] == 'W' || commands[i][0] == 'r' ||
					commands[i][0] == 't' || commands[i][0] == 'u' || commands[i][0] == 'c')
		{
			// -w, -W, -r, t, u, c take an argument
			if (arguments[i][0] == '\0') return 1;
			continue;
		}

		// checking whether the commands given
		// are a valid input sequence

		if (commands[i][0] == 'l') // if a file is to be locked by client
		{
			// client must either remove or unlock the file after
			char* tmp = arguments[i]; // name of file to be locked
			int ok = 0; // resetting ok
			for (int j = i+1; j < len; j++)
			{
				if (commands[j][0] == 'c' || commands[j][0] == 'u')
				{
					// check whether the file has the same name
					if (strcmp(tmp, arguments[j]) == 0)
					{
						ok = 1;
						break;
					}
				}
			}
			if (!ok) return 1;
			continue;
		}
		if (commands[i][0] == 'd') // if files are to be saved
		{
			// there must be a reading operation before
			if (i == 0) return 1;
			if (commands[i-2][0] != 'r' && commands[i-2][0] != 'R' &&
						commands[i-1][0] != 'R')
				return 1;
			continue;
		}
		if (commands[i][0] == 'D') // if files are to be saved
		{
			// there must be a writing operation before
			if (i == 0) return 1;
			if (commands[i-2][0] != 'w' && commands[i-2][0] != 'W')
				return 1;
			continue;
		}
	}
	return 0;
}