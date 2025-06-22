/**
 * filename: log.c
 * description: Handels logging and unloading logs
 */

#pragma once

// C includes
#include <stddef.h>

typedef enum log_status_e
{
    LOG_OK = 0,
    LOG_FAILED,
    LOG_INVALID,
    LOG_NOMEM
} log_status_t;

/**
 * Initializes the logger by opening the given file path.
 * Creates/truncates the file for appending.
 */
log_status_t log_init(const char *path);

/** Closes the log file descriptor if open. */
void log_destroy(void);

/** Logs an info message with formatting. */
log_status_t log_str(const char *format, ...);

/**
 * Reads entire log file content into a dynamicly allocated buffer.
 * Caller must free. Rewinds file before reading.
 */
log_status_t log_read_all(uint8_t **out_buf, size_t *out_size);


// Logging Macros
// note: the macros are wrapped in do while in order to be able to add `;` after calling them
// e.g INFO(...);

#define LOG(fmt, ...) \
    do { (void)log_str(" %-12s | %-4d | %-18s| "fmt, __FILE__, __LINE__, __func__, ##__VA_ARGS__); } while(0)

#define INFO(fmt, ...) \
    LOG("[INFO] "fmt, ##__VA_ARGS__)

#define ERROR(label, fmt, ...) \
    do { \
        LOG("[ERR]  " fmt, ##__VA_ARGS__); \
        goto label; \
    } while(0)

#define ASSERT_RET_EQ(expected_code, result_code, label, fmt, ...) \
    do { \
        if ((expected_code) != (result_code)) { \
            ERROR(label, "(EXPECTED: %d, RETCODE: %d)    "fmt, (expected_code), (result_code), ##__VA_ARGS__); \
        } \
    } while(0)

#define ASSERT_RET_NE(expected_code, result_code, label, fmt, ...) \
    do { \
        if ((expected_code) == (result_code)) { \
            ERROR(label, "(UNEXPECTED RET: %d)    "fmt, (result_code), ##__VA_ARGS__); \
        } \
    } while(0)

#define ASSERT_NOT_NULL(result, label, fmt, ...) \
    do { \
        if (NULL == (result)) { \
            ERROR(label, fmt, ##__VA_ARGS__); \
        } \
    } while(0)



