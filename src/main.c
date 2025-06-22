// C includes
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

// User includes
#include "file.h"
#include "log.h"
#include "exec.h"

int test_file()
{
    const char *test_path = "/tmp/test_output.bin";
    const uint8_t message[] = "Hello, file I/O!";
    const size_t message_len = sizeof(message) - 1; // exclude null terminator

    int fd = -1;
    file_status_t status = FILE_OK;

    // ---- Write Phase ----
    fd = open(test_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0)
    {
        perror("open for write");
        return 1;
    }

    status = write_all(fd, message, message_len);
    if (status != FILE_OK)
    {
        fprintf(stderr, "write_all failed: %d\n", status);
        close(fd);
        return 1;
    }

    close(fd);

    uint8_t *buffer = NULL;
    size_t size = 0;

    status = read_file_from_path(test_path, &buffer, &size);
    close(fd);

    if (status != FILE_OK)
    {
        fprintf(stderr, "read_file failed: %d\n", status);
        return 1;
    }

    // ---- Output ----
    printf("Read back (%zu bytes): ", size);
    fwrite(buffer, 1, size, stdout);
    printf("\n");

    free(buffer);
    return 0;
}

void test_log(void)
{
    log_status_t status = LOG_OK;

    uint8_t *log_buf = NULL;
    size_t log_len = 0;
    int a = -1;

    // 1. INFO without param
    INFO("System initialized");

    // 2. INFO with param
    INFO("INFO, %d", 1);

    // 5. ERR_IF_FAILED that does NOT jump
    status = LOG_OK;
    ASSERT_RET_EQ(LOG_OK, status, skip, "ASSERT_RET_EQ that doesnt jmp, num: %d", 2);

    // 6. ERR_IF_FAILED that DOES jump
    status = LOG_FAILED;
    ASSERT_RET_EQ(LOG_OK, status, skip, "ASSERT_RET_EQ that does jmp, string: %s", "hello");

    // This line should not be reached
    INFO("You should NOT see this");

skip:
    INFO("after jmp");
    // 7. Read back and print log with \0 replaced by \n

    if (log_read_all(&log_buf, &log_len) == LOG_OK && log_buf != NULL)
    {
        for (size_t i = 0; i < log_len; ++i)
        {
            if (log_buf[i] == '\0')
            {
                if (i != log_len - 1)
		{
                    log_buf[i] = '\n';
		}
            }
        }
	printf("%s\n", log_buf);
        free(log_buf);
    }
    else
    {
        printf("failed to read logs %d \n", a);
    }
}

void test_exec(void)
{
    size_t stdout_size = 0;
    char *stdout_buf = NULL;
    size_t stderr_size = 0;
    char *stderr_buf = NULL;
    int exit_code = -1;
    exec_status_t status = EXEC_FAILED;

    // Test 1: stdout output with /bin/echo
    {
        char *args[] = { "/bin/echo", "hello world", NULL };

        INFO("Running stdout test: /bin/echo \"hello world\"");

        status = exec_run(args[0], args, &stdout_size, &stdout_buf, &stderr_size, &stderr_buf, &exit_code, 1000);
	if (NULL != stdout_buf)
	{
	    stdout_buf = realloc(stdout_buf, stdout_size + 1);
	    stdout_buf[stdout_size + 1] = '\0';
	}
	if (NULL != stderr_buf)
	{
	    stderr_buf = realloc(stderr_buf, stdout_size + 1);
	    stderr_buf[stderr_size + 1] = '\0';
	}
        ASSERT_RET_EQ(EXEC_OK, status, cleanup, "exec_run failed for echo: status=%d", status);
		
        printf("Exit Code: %d\n", exit_code);
        printf("STDOUT: %s\n", stdout_buf);
        printf("STDERR (should be empty): %s\n", stderr_buf ? stderr_buf : "(null)");

        free(stdout_buf);
        free(stderr_buf);
        stdout_buf = NULL;
        stderr_buf = NULL;
    }
    printf("\n\n\n");
    // Test 2: stderr output with ls on nonexistent path
    {
        char *args[] = { "/bin/ls", "/nonexistent_path", NULL };

        INFO("Running stderr test: /bin/ls /nonexistent_path");

        status = exec_run(args[0], args, &stdout_size, &stdout_buf, &stderr_size, &stderr_buf, &exit_code, 1000);
	if (NULL != stdout_buf)
	{
	    stdout_buf = realloc(stdout_buf, stdout_size + 1);
	    stdout_buf[stdout_size + 1] = '\0';
	}
	if (NULL != stderr_buf)
	{
	    stderr_buf = realloc(stderr_buf, stdout_size + 1);
	    stderr_buf[stderr_size + 1] = '\0';
	}

        ASSERT_RET_EQ(EXEC_OK, status, cleanup, "exec_run failed for ls: status=%d", status);

	printf("Exit Code: %d\n", exit_code);
        printf("STDOUT (should be empty): %s\n", stdout_buf ? stdout_buf : "(null)");
        printf("STDERR: %s\n", stderr_buf);

        free(stdout_buf);
        free(stderr_buf);
        stdout_buf = NULL;
        stderr_buf = NULL;
    }

cleanup:
    if (NULL != stdout_buf)
    {
        free(stdout_buf);
    }

    if (NULL != stderr_buf)
    {
        free(stderr_buf);
    }
}

int main(void)
{
    for (int i = 0; i < 3; i++) {

        if (log_init("/tmp/logs") != LOG_OK)
        {
            return 1;
        }
	printf("\n\n----------------------------------------------------test_file------------------------------------------------------------\n\n");
	test_file();
	printf("\n\n----------------------------------------------------test_exec------------------------------------------------------------\n\n");
	test_exec();
	printf("\n\n----------------------------------------------------test_log-------------------------------------------------------------\n\n");
	test_log();
        log_destroy();
    }
    return 0;
}


