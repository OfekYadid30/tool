/**
 * filename: file.h
 * description: Implementation of minimal I/O helper functions.
 */

#pragma once

// C includes
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

typedef enum file_status_e
{
    FILE_OK = 0,
    FILE_FAILED,
    FILE_EOF,
    FILE_NOMEM,
    FILE_UNSUPPORTED,
    FILE_EMPTY,
    FILE_INACCESSIBLE,
} file_status_t;

/**
 * Attempts to read up to `count` bytes from `fd` into `buf`.
 *
 * @param fd              A valid file descriptor open for reading.
 * @param buf             Buffer to read into.
 * @param count           Max number of bytes to read.
 * @param out_bytes_read  Actual number of bytes read.
 * @return file_status_t (FILE_OK on success)
 */
file_status_t read_partial(int fd, uint8_t *buf, size_t count, size_t *out_bytes_read);

/**
 * Attempts to write up to `count` bytes from `buf` to `fd`.
 *
 * @param fd                 A valid file descriptor open for writing.
 * @param buf                Buffer to write from.
 * @param count              Max number of bytes to write.
 * @param out_bytes_written  Actual number of bytes written.
 * @return file_status_t (FILE_OK on success)
 */
file_status_t write_partial(int fd, const uint8_t *buf, size_t count, size_t *out_bytes_written);

/**
 * Reads exactly `count` bytes from `fd` into `buf`.
 * NOTE: if the function succeed, the cursor is set
 * to end of file
 *
 * @param fd     File descriptor open for reading.
 * @param buf    Buffer to fill.
 * @param count  Total number of bytes to read.
 * @return file_status_t (FILE_OK on success)
 */
file_status_t read_all(int fd, uint8_t *buf, size_t count);

/**
 * Writes exactly `count` bytes from `buf` to `fd`.
 *
 * @param fd     File descriptor open for writing.
 * @param buf    Buffer to write.
 * @param count  Total number of bytes to write.
 * @return file_status_t (FILE_OK on success)
 */
file_status_t write_all(int fd, const uint8_t *buf, size_t count);

/**
 * read_until_eof:
 * Reads all available data from `fd` until EOF.
 * Dynamically allocates a buffer that must be freed by the caller.
 *
 * @param fd           File descriptor to read from.
 * @param out_buffer   Pointer to buffer pointer which will receive
 *                     dynamically allocated data buffer on success.
 *                     Caller is responsible for freeing this buffer.
 * @param out_bytes_read Number of bytes read and stored in out_buffer.
 *
 * @return file_status_t (FILE_OK on success)
 */
file_status_t read_until_eof(int fd, uint8_t **out_buffer, size_t *out_bytes_read);

/**
 * Reads an entire regular file into memory.
 *
 * @param fd        File descriptor of regular file.
 * @param out_buf   Output buffer pointer (dynamicly allocated, caller must free).
 * @param out_size  Output size in bytes.
 * @return file_status_t (FILE_OK on success)
 */
file_status_t read_file(int fd, uint8_t **out_buf, size_t *out_size);

/**
 * Reads an entire regular file into memory.
 *
 * @param path	    The path for the file
 * @param out_buf   Output buffer pointer (dynamicly allocated, caller must free).
 * @param out_size  Output size in bytes.
 * @return file_status_t (FILE_OK on success)
 */
file_status_t read_file_from_path(const char *path, uint8_t **out_buf, size_t *out_size);


