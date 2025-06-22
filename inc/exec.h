/**
 * filename: exec.h
 * description: Executes an external process, captures its output and exit code.
 */

#pragma once

// C includes
#include <stddef.h>

typedef enum exec_status_e
{
    EXEC_OK = 0,
    EXEC_FAILED,
    EXEC_INVALID,
    EXEC_TIMEOUT,
    EXEC_INACCESSIBLE,
} exec_status_t;

/**
 * Runs an executable with arguments and captures its stdout and stderr output.
 *
 * @param path           Path to executable.
 * @param argv           Argument vector (argv[0] should be the executable name).
 * @param out_stdout     Pointer to hold allocated stdout buffer (must be freed by caller).
 * @param out_stderr     Pointer to hold stderr buffer (must be freed by caller).
 * @param out_exit_code  Pointer to hold process exit code.
 * @param timeout_ms 	 Timeout for the command in miliseconds.
 * @return exec_status_t Execution status (EXEC_OK on success).
 */
exec_status_t exec_run(const char *path, char **args, size_t *out_stdout_size,
		       char **out_stdout, size_t *out_stderr_size, char **out_stderr,
		       int *out_exit_code, unsigned int timeout_ms);
