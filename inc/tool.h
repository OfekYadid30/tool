/**
 * filename: toll.h
 * description: defines tool properites and configurations
 */

#pragma once

// C includes
#include <stdint.h>

// defines
#define TOOL_NAME_SIZE 4
#define IP_STR_MAX_LEN 16

typedef struct tool_conf_s
{
    char ip[IP_STR_MAX_LEN];
    uint16_t port;
    uint32_t default_sleep;
} tool_conf_t;

typedef struct tool_s
{
    char name[TOOL_NAME_SIZE];
    tool_conf_t conf;
} tool_t;
