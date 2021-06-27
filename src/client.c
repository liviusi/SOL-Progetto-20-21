/**
 * @brief
 * @author Giacomo Trapani.
*/

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COMMANDLEN 2
#define ARGUMENTLEN 2048

#define VALID_COMMAND(character) \
	character == 'h' || character == 'f' || character == 'w' || \
	character == 'W' || character == 'd' || character == 'D' || \
	character == 'r' || character == 'R' || character == 't' || \
	character == 'l' || character == 'u' || character == 'c' || \
	character == 'p'

int
validate(char** commands, char** arguments, int len);

bool h_set = false;

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
	char** commands = (char**) malloc(sizeof(char*) * (argc - 1));
	if (!commands) return 1;
	for (int i = 0; i < argc - 1; i++)
	{
		commands[i] = (char*) malloc(sizeof(char) * COMMANDLEN);
		if (!commands[i]) return 1;
		memset(commands[i], 0, COMMANDLEN);
	}
	char** arguments = (char**) malloc(sizeof(char*) * (argc - 1));
	if (!arguments) return 1;
	for (int i = 0; i < argc - 1; i++)
	{
		arguments[i] = (char*) malloc(sizeof(char) * ARGUMENTLEN);
		if (!arguments[i]) return 1;
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
	for (int i = 1; i < argc - 1; i++)
	{
		if (commands[i-1][0] != '\0') continue;
		snprintf(arguments[i-2], ARGUMENTLEN, "%s", argv[i]);
	}
	int err = validate(commands, arguments, argc-1);
	if (err == 1) fprintf(stdout, "validate: NOT VALID.\n");
	else fprintf(stdout, "validate: VALID.\n");
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

		// checking whether the commands given
		// are a valid input sequence
		if (commands[i][0] == 'h') h_set = true;
		bool found_r = false; // toggled on when r/R belongs to commands
		bool found_w = false; // toggled on when w/W belongs to commands

	}
	return 0;
}