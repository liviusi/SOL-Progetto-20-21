#ifndef _WRAPPERS_H_
#define _WRAPPERS_H_

#include <server_defines.h>
#include <stdarg.h>

/**
 * @brief Returns fatal error if called function output value is not equal to expected value.
 * @param variable will be set to function_call output value
 * @param expected_value value returned by function_call in case of success
 * @param function_call actual function call
*/
#define RETURN_FATAL_IF_NEQ(variable, expected_value, function_call) \
	if ((variable = function_call) != expected_value) {fprintf(stderr, "[%s:%d] Fatal error occurred.", __FILE__, __LINE__); return OP_FATAL; }

/**
 * @brief Returns fatal error if called function output value is equal to expected value.
 * @param variable will be set to function_call output value
 * @param expected_value value returned by function_call in case of failure
 * @param function_call actual function call
*/
#define RETURN_FATAL_IF_EQ(variable, expected_value, function_call) \
	if ((variable = function_call) == expected_value) {fprintf(stderr, "[%s:%d] Fatal error occurred.", __FILE__, __LINE__); return OP_FATAL; }

/**
 * @brief Exits with EXIT_FAILURE if called function output value is not equal to expected value.
 * @param variable will be set to function_call output value
 * @param expected_value value returned by function_call in case of success
 * @param function_call actual function call
*/
#define EXIT_IF_NEQ(variable, expected_value, function_call) \
	if ((variable = function_call) != expected_value) exit(EXIT_FAILURE);

/**
 * @brief Goes to label if variable is not equal to value.
 * @param errnosave will be used to store errno value
*/
#define GOTO_LABEL_IF_NEQ(variable, value, errnosave, label) \
	if (variable != value) \
	{ \
		errnosave = errno; \
		goto label; \
	}

/**
 * @brief Goes to label if variable is equal to value.
 * @param errnosave will be used to store errno value
*/
#define GOTO_LABEL_IF_EQ(variable, value, errnosave, label) \
	if (variable == value) \
	{ \
		errnosave = errno; \
		goto label; \
	}

/**
 * @brief Prints to stdout if condition is true.
 * @param flag condition to be evaluated
*/
#define PRINT_IF(flag, ...) \
	if (flag) fprintf(stdout, __VA_ARGS__);

#define READN_NUMBER(fd, buffer, bufferlen, number, err, label) \
	memset(buffer, 0, bufferlen); \
	if (readn(fd, (void*) buffer, bufferlen) == -1) \
	{ \
		err = errno; \
		goto label; \
	} \
	if (isNumber(buffer, &number) != 0) \
	{ \
		err = EBADMSG; \
		goto label; \
	}

#endif