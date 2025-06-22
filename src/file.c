/**
 * filename: file.c
 * description: Implementation of minimal I/O helper functions.
 */

// C includes
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

// User includes
#include "file.h"
#include "log.h"

// Defines
#define READ_CHUNK_SIZE 4096

file_status_t read_partial(int fd, uint8_t *buffer, size_t requested_count, size_t *out_bytes_read)
{
    file_status_t status = FILE_FAILED;
    ssize_t sys_bytes = -1;

    ASSERT_NOT_NULL(buffer, cleanup, "buffer is NULL");
    ASSERT_NOT_NULL(out_bytes_read, cleanup, "out_bytes_read is NULL");

    *out_bytes_read = 0;

    do {
        sys_bytes = read(fd, buffer, requested_count);
    } while (-1 == sys_bytes && (EINTR == errno || EAGAIN == errno));

    ASSERT_RET_NE(-1, sys_bytes, cleanup, "read() failed: %s", strerror(errno));

    *out_bytes_read = (size_t)sys_bytes;
    status = FILE_OK;

cleanup:
    return status;
}

file_status_t write_partial(int fd, const uint8_t *buffer, size_t requested_count, size_t *out_bytes_written)
{
    file_status_t status = FILE_FAILED;
    ssize_t sys_bytes = -1;

    ASSERT_NOT_NULL(buffer, cleanup, "buffer is NULL");
    ASSERT_NOT_NULL(out_bytes_written, cleanup, "out_bytes_written is NULL");

    *out_bytes_written = 0;

    do {
        sys_bytes = write(fd, buffer, requested_count);
    } while (-1 == sys_bytes && (EINTR == errno || EAGAIN == errno));

    ASSERT_RET_NE(-1, sys_bytes, cleanup, "write() failed: %s", strerror(errno));

    *out_bytes_written = (size_t)sys_bytes;
    status = FILE_OK;

cleanup:
    return status;
}

file_status_t read_all(int fd, uint8_t *buffer, size_t total_count)
{
    file_status_t status = FILE_FAILED;
    size_t total_read = 0;
    size_t chunk = 0;

    ASSERT_NOT_NULL(buffer, cleanup, "buffer is NULL");

    while (total_read < total_count)
    {
        status = read_partial(fd, buffer + total_read, total_count - total_read, &chunk);
        ASSERT_RET_EQ(FILE_OK, status, cleanup, "read_partial failed");

        if (0 == chunk)
	{
            INFO("EOF reached before full read");
            status = FILE_EOF;
            goto cleanup;
        }
        total_read += chunk;
    }

    status = FILE_OK;
cleanup:
    return status;
}

file_status_t write_all(int fd, const uint8_t *buffer, size_t total_count)
{
    file_status_t status = FILE_FAILED;
    size_t total_written = 0;
    size_t chunk = 0;
	
    ASSERT_NOT_NULL(buffer, cleanup, "buffer is NULL");

    while (total_written < total_count)
    {
        status = write_partial(fd, buffer + total_written, total_count - total_written, &chunk);
        ASSERT_RET_EQ(FILE_OK, status, cleanup, "write_partial failed");
        total_written += chunk;
    }

    status = FILE_OK;
cleanup:
    return status;
}

file_status_t read_until_eof(int fd, uint8_t **out_buffer, size_t *out_bytes_read)
{
    file_status_t status = FILE_FAILED;
    uint8_t *buffer = NULL;
    uint8_t *tmp = NULL;
    size_t capacity = 0;
    size_t total_read = 0;
    size_t chunk = 0;
    size_t new_capacity = 0;

    ASSERT_NOT_NULL(out_buffer, cleanup, "out_buffer is NULL");
    ASSERT_NOT_NULL(out_bytes_read, cleanup, "out_bytes_read is NULL");

    *out_buffer = NULL;
    *out_bytes_read = 0;

    capacity = READ_CHUNK_SIZE;
    buffer = malloc(capacity);
    ASSERT_NOT_NULL(buffer, cleanup, "malloc failed, %s", strerror(errno));

    while (1)
    {
        if (capacity - total_read < READ_CHUNK_SIZE)
	{
	    if (capacity > SIZE_MAX - READ_CHUNK_SIZE)
	    {
		ERROR(cleanup, "too many bytes to read! couldn't reach EOF");
	    }

            new_capacity = capacity + READ_CHUNK_SIZE;

            tmp = realloc(buffer, new_capacity);
            ASSERT_NOT_NULL(tmp, cleanup, "realloc failed: %s", strerror(errno));

            buffer = tmp;
            capacity = new_capacity;
        }

        status = read_partial(fd, buffer + total_read, READ_CHUNK_SIZE, &chunk);
        ASSERT_RET_EQ(FILE_OK, status, cleanup, "read_partial failed");

        if (0 == chunk)
	{
            INFO("EOF reached");
            break;
        }
        total_read += chunk;
    }

    if (total_read > 0)
    {
        tmp = realloc(buffer, total_read);
        ASSERT_NOT_NULL(tmp, cleanup, "realloc failed: %s", strerror(errno));
        buffer = tmp;
    }
    else
    {
        free(buffer);
        buffer = NULL;
    }

    *out_buffer = buffer;
    *out_bytes_read = total_read;
    buffer = NULL;

    status = FILE_OK;
cleanup:
    if (NULL != buffer) {
        free(buffer);
    }
    return status;
}

file_status_t read_file(int fd, uint8_t **out_buffer, size_t *out_size)
{
    file_status_t status = FILE_FAILED;
    struct stat file_stat = {0};
    size_t file_size = 0;
    uint8_t *buffer = NULL;

    ASSERT_NOT_NULL(out_buffer, cleanup, "out_buffer is NULL");
    ASSERT_NOT_NULL(out_size, cleanup, "out_size is NULL");

    *out_buffer = NULL;
    *out_size = 0;

    ASSERT_RET_NE(-1, fstat(fd, &file_stat), cleanup, "fstat failed: %s", strerror(errno));
	
    ASSERT_RET_NE(0, S_ISREG(file_stat.st_mode), cleanup, "descriptor is not a regular file");

    if (file_stat.st_size < 0 || (uintmax_t)file_stat.st_size > SIZE_MAX)
    {
	ERROR(cleanup, "invalid file size");
    }
    else if (0 == file_stat.st_size)
    {
        INFO("file size is zero");
        status = FILE_EMPTY;
        goto cleanup;
    }

    // now its safe to cast
    file_size = (size_t)file_stat.st_size;

    buffer = malloc(file_size);
    ASSERT_NOT_NULL(buffer, cleanup, "malloc failed: %s", strerror(errno));

    ASSERT_RET_NE(-1, lseek(fd, 0, SEEK_SET), cleanup, "lseek failed");

    status = read_all(fd, buffer, file_size);
    ASSERT_RET_EQ(FILE_OK, status, cleanup, "read_all failed");

    *out_buffer = buffer;
    *out_size = file_size;
    buffer = NULL;

    status = FILE_OK;
cleanup:
    if (FILE_OK != status && NULL != buffer) {
        free(buffer);
	buffer = NULL;
    }
    return status;
}


file_status_t read_file_from_path(const char *path, uint8_t ** out_buffer, size_t * out_size) 
{	
    int fd = -1;
    file_status_t status = FILE_FAILED;

    ASSERT_NOT_NULL(path, cleanup, "path is NULL");
    ASSERT_NOT_NULL(out_buffer, cleanup, "out_buffer is NULL");
    ASSERT_NOT_NULL(out_size, cleanup, "out_size is NULL");
	
    fd = open(path, O_RDONLY | O_NONBLOCK);
    if (-1 == fd)
    {
	status = FILE_INACCESSIBLE;
	ERROR(cleanup, "open failed: %s", strerror(errno));
    }

    status = read_file(fd, out_buffer, out_size);
cleanup:
    if (fd > 0)
    {
	close(fd);
    }
    return status;
}
