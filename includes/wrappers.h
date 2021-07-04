/**
 * @brief Useful macros.
 * @author Giacomo Trapani.
*/

#ifndef _WRAPPERS_H_
#define _WRAPPERS_H_

#include <errno.h>
#include <stdarg.h>

#include <server_defines.h>

#define MBYTE 0.000001f

/**
 * @brief Returns fatal error if called function output value is not equal to expected value. It also prints "errno" value
 * and reports where the error occurred.
 * @param variable will be set to function_call output value
 * @param expected_value value returned by function_call in case of success
 * @param function_call actual function call
*/
#define RETURN_FATAL_IF_NEQ(variable, expected_value, function_call) \
	if ((variable = function_call) != expected_value) { fprintf(stderr, "[%s:%d] Fatal error occurred. errno = %d\n", __FILE__, __LINE__, errno); return OP_FATAL; }

/**
 * @brief Returns fatal error if called function output value is equal to expected value. It also prints "errno" value
 * and reports where the error occurred.
 * @param variable will be set to function_call output value
 * @param expected_value value returned by function_call in case of failure
 * @param function_call actual function call
*/
#define RETURN_FATAL_IF_EQ(variable, expected_value, function_call) \
	if ((variable = function_call) == expected_value) { fprintf(stderr, "[%s:%d] Fatal error occurred. errno = %d\n", __FILE__, __LINE__, errno); return OP_FATAL; }

/**
 * @brief Exits with EXIT_FAILURE if called function output value is not equal to expected value.
 * @param variable will be set to function_call output value
 * @param expected_value value returned by function_call in case of success
 * @param function_call actual function call
*/
#define EXIT_IF_NEQ(variable, expected_value, function_call, function_name) \
	if ((variable = function_call) != expected_value) \
	{ \
		fprintf(stderr, "Fatal error occurred at %s:%d\n", __FILE__, __LINE__); \
		perror(#function_name); \
		exit(EXIT_FAILURE); \
	}

/**
 * @brief Exits with EXIT_FAILURE if called function output value is equal to expected value.
 * @param variable will be set to function_call output value
 * @param expected_value value returned by function_call in case of success
 * @param function_call actual function call
*/
#define EXIT_IF_EQ(variable, expected_value, function_call, function_name) \
	if ((variable = function_call) == expected_value) \
	{ \
		fprintf(stderr, "Fatal error occurred at %s:%d\n", __FILE__, __LINE__); \
		perror(#function_name); \
		exit(EXIT_FAILURE); \
	}
/**
 * @brief Goes to label if variable is not equal to value.
 * @param errnocopy will be used to store errno value
*/
#define GOTO_LABEL_IF_NEQ(variable, value, errnocopy, label) \
	if (variable != value) \
	{ \
		errnocopy = errno; \
		goto label; \
	}

/**
 * @brief Goes to label if variable is equal to value.
 * @param errnocopy will be used to store errno value
*/
#define GOTO_LABEL_IF_EQ(variable, value, errnocopy, label) \
	if (variable == value) \
	{ \
		errnocopy = errno; \
		goto label; \
	}

/**
 * @brief Prints to stdout if condition is true.
 * @param flag condition to be evaluated
*/
#define PRINT_IF(flag, ...) \
	if (flag) fprintf(stdout, __VA_ARGS__);

/**
 * @brief Gets maximum value between given params.
*/
#define MAX(a, b) ((a >= b) ? (a) : (b))

#endif