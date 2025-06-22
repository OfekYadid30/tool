/**
 * filename: log.c
 * description: Handels logging and unloading logs
 */

// C includes
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>

// User includes
#include "log.h"
#include "file.h"

// defines
#define LOG_BUFFER_SIZE 350

static int g_log_fd = -1;
static int g_logging_in_process = 0;

static log_status_t log_write(const char *format, va_list args)
{
    log_status_t status = LOG_FAILED;
    char buffer[LOG_BUFFER_SIZE] = {0};
    int written = 0;

    if (g_log_fd < 0)
    {
        goto cleanup;
    }
    else if (g_logging_in_process)
    {
	goto done;
    }

    g_logging_in_process = 1;

    if (NULL == format)
    {
        status = LOG_INVALID;
        goto cleanup;
    }

    written = vsnprintf(buffer, sizeof(buffer), format, args);

    if (written < 0 || (size_t)written >= sizeof(buffer))
    {
    	// if written is less than zero, couldn't write anything
    	// if written is more than buffer, buff is too small 
        goto cleanup;
    }

    // in log every lines are seperated by `\0` rather than `\n`
    for (int i = 0; i < written; ++i)
    {
        if (buffer[i] == '\n')
        {
            buffer[i] = '\0';
        }
    }
	
    // each write is a new line, and lines are seperated by \0, so write written + 1
    // so the \0 will be written
    if (FILE_OK != write_all(g_log_fd, (const uint8_t *)buffer, (size_t)(written + 1)))
    {
        goto cleanup;
    }
	
    status = LOG_OK;

cleanup:
    g_logging_in_process = 0;
done:
    return status;
}

log_status_t log_init(const char *path)
{
    log_status_t status = LOG_FAILED;

    if (NULL == path)
    {
        status = LOG_INVALID;
        goto cleanup;
    }

    g_log_fd = open(path, O_RDWR | O_CREAT | O_TRUNC | O_APPEND | O_CLOEXEC);
    if (g_log_fd < 0)
    {
        goto cleanup;
    }

    status = LOG_OK;

cleanup:
    return status;
}

void log_destroy()
{
    if (g_log_fd > 0)
    {
        close(g_log_fd);
        g_log_fd = -1;
    }
}

log_status_t log_str(const char *format, ...)
{
    log_status_t status = LOG_FAILED;
    va_list args;

    if (NULL == format)
    {
        status = LOG_INVALID;
        goto cleanup;
    }

    va_start(args, format);
    status = log_write(format, args);
    va_end(args);

cleanup:
    return status;
}

log_status_t log_read_all(uint8_t **out_buf, size_t *out_size)
{
    log_status_t status = LOG_FAILED;

    if (g_log_fd < 0 || NULL == out_buf || NULL == out_size)
    {
        status = LOG_INVALID;
        goto cleanup;
    }

    if (FILE_OK != read_file(g_log_fd, out_buf, out_size))
    {
        goto cleanup;
    }

    status = LOG_OK;

cleanup:
    return status;
}

