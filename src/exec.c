/**
 * filename: exec.c
 * description: Executes an external process, captures its output and exit code.
 */

// C includes
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>

// User includes
#include "exec.h"
#include "log.h"
#include "file.h"

// defines
#define STEP_MS 10

typedef enum pipe_end_e {
    PIPE_READ = 0,
    PIPE_WRITE = 1,
    PIPE_SIZE = 2
} pipe_end_t;

static void exec_child(int stdout_pipe[PIPE_SIZE],
                       int stderr_pipe[PIPE_SIZE],
                       const char *path,
                       char **args)
{
    int ret_val = -1;

    ASSERT_NOT_NULL(path, failed, "path is NULL");
    ASSERT_NOT_NULL(args, failed, "args is NULL");

    close(stdout_pipe[PIPE_READ]);
    stdout_pipe[PIPE_READ] = -1;
    close(stderr_pipe[PIPE_READ]);
    stderr_pipe[PIPE_READ] = -1;

    ret_val = dup2(stdout_pipe[PIPE_WRITE], STDOUT_FILENO);
    ASSERT_RET_NE(-1, ret_val, failed, "couldn't dup stdout: %s", strerror(errno));

    ret_val = dup2(stderr_pipe[PIPE_WRITE], STDERR_FILENO);
    ASSERT_RET_NE(-1, ret_val, failed, "couldn't dup stderr: %s", strerror(errno));

    close(stdout_pipe[PIPE_WRITE]);
    close(stderr_pipe[PIPE_WRITE]);

    execvp(path, args);
	
    // not expected to get here
failed:
    _exit(127);
}

static exec_status_t exec_wait_child(pid_t pid,
                                     unsigned int timeout_ms,
                                     int *wstatus)
{
    exec_status_t status = EXEC_FAILED;
    unsigned int waited_ms = 0;

    ASSERT_NOT_NULL(wstatus, cleanup, "wstatus is NULL");

    while (0 == waitpid(pid, wstatus, WNOHANG))
    {
        if (waited_ms >= timeout_ms)
	{
            INFO("Timeout reached, killing child process");

	    ASSERT_RET_NE(-1, kill(pid, SIGKILL), cleanup, "kill failed: %s", strerror(errno));
    	    ASSERT_RET_NE(-1, waitpid(pid, wstatus, 0), cleanup, "waitpid failed");

            status = EXEC_TIMEOUT;
            goto cleanup;
        }

        usleep(STEP_MS * 1000);
        waited_ms += STEP_MS;
    }

    status = EXEC_OK;
cleanup:
    return status;
}

static exec_status_t check_executable_access(const char *path)
{
    exec_status_t status = EXEC_FAILED;
    struct stat st = {0};
    int ret = -1;

    ASSERT_RET_NE(NULL, path, cleanup, "invalid path: NULL");

    ret = access(path, X_OK);
    ASSERT_RET_EQ(0, ret, cleanup, "access(%s, X_OK) failed: %s", path, strerror(errno));

    ret = stat(path, &st);
    ASSERT_RET_EQ(0, ret, cleanup, "stat(%s) failed: %s", path, strerror(errno));

    if (!S_ISREG(st.st_mode)) {
        status = EXEC_INACCESSIBLE;
        goto cleanup;
    }

    status = EXEC_OK;

cleanup:
    return status;
}

exec_status_t exec_run(const char *path,
                       char ** args,
                       size_t *out_stdout_size,
                       char **out_stdout,
                       size_t *out_stderr_size,
                       char **out_stderr,
                       int *out_exit_code,
                       unsigned int timeout_ms)
{
    exec_status_t status = EXEC_FAILED;
    exec_status_t access_status = EXEC_FAILED;

    int stdout_pipe[PIPE_SIZE] = { -1, -1 };
    int stderr_pipe[PIPE_SIZE] = { -1, -1 };
    pid_t pid = -1;
    int wstatus = 0;


    uint8_t *stdout_buf = NULL;
    uint8_t *stderr_buf = NULL;

    file_status_t file_status = FILE_FAILED;

    ASSERT_NOT_NULL(path, cleanup, "path is NULL");
    ASSERT_NOT_NULL(args, cleanup, "args is NULL");
    ASSERT_NOT_NULL(out_stdout_size, cleanup, "out_stdout_size is NULL");
    ASSERT_NOT_NULL(out_stdout, cleanup, "out_stdout is NULL");
    ASSERT_NOT_NULL(out_stderr_size, cleanup, "out_stderr_size is NULL");
    ASSERT_NOT_NULL(out_stderr, cleanup, "out_stderr is NULL");
    ASSERT_NOT_NULL(out_exit_code, cleanup, "out_exit_code is NULL");
    
    access_status = check_executable_access(path);
    if (EXEC_INACCESSIBLE == access_status)
    {
	status = EXEC_INACCESSIBLE;
    }
    ASSERT_RET_EQ(EXEC_OK, access_status, cleanup, "check_executable_access failed");

    INFO("Creating stdout and stderr pipes");
    ASSERT_RET_EQ(0, pipe(stdout_pipe), cleanup, "pipe stdout failed: %s", strerror(errno));
    ASSERT_RET_EQ(0, pipe(stderr_pipe), cleanup, "pipe stderr failed: %s", strerror(errno));

    INFO("Forking process to exec: %s", path);
    pid = fork();
    ASSERT_RET_NE(-1, pid, cleanup, "fork failed: %s", strerror(errno));

    if (0 == pid) {
        exec_child(stdout_pipe, stderr_pipe, path, args); /* never returns */
    }

    /* parent */
    close(stdout_pipe[PIPE_WRITE]);
    stdout_pipe[PIPE_WRITE] = -1;
    close(stderr_pipe[PIPE_WRITE]);
    stderr_pipe[PIPE_WRITE] = -1;


    INFO("Waiting for child process to finish");
    if (EXEC_TIMEOUT == exec_wait_child(pid, timeout_ms, &wstatus)) {
        status = EXEC_TIMEOUT;
        goto cleanup;
    }

    INFO("Reading stdout from child");
    file_status = read_until_eof(stdout_pipe[PIPE_READ], &stdout_buf, out_stdout_size);
    ASSERT_RET_EQ(FILE_OK, file_status, cleanup, "read stdout failed: %d", file_status);

    INFO("Reading stderr from child");
    file_status = read_until_eof(stderr_pipe[PIPE_READ], &stderr_buf, out_stderr_size);
    ASSERT_RET_EQ(FILE_OK, file_status, cleanup, "read stderr failed: %d", file_status);

    *out_exit_code = (0 != WIFEXITED(wstatus)) ? WEXITSTATUS(wstatus) : -1;

    // OK to cast from uint8_t* to char *
    *out_stdout = (char *)stdout_buf;
    *out_stderr = (char *)stderr_buf;
    stdout_buf = NULL;
    stderr_buf = NULL;

    status = EXEC_OK;

cleanup:
    if (-1 != stdout_pipe[PIPE_READ]) {
        close(stdout_pipe[PIPE_READ]);
    }
    if (-1 != stderr_pipe[PIPE_READ]) {
        close(stderr_pipe[PIPE_READ]);
    }

    free(stdout_buf);
    free(stderr_buf);

    return status;
}
