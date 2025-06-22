/**
 * filename: core.c
 * description: handle commands and sleep in a loop
 */

// C includes
#include <unistd.h>

// User includes
#include "core.h"
#include "network.h"
#include "log.h"

// defines
#define LOG_FILE_PATH ("/tmp/logs")

void run(const tool_t *tool)
{
    unsigned int sleep_duration = 0;
    int should_die = 0;
    network_status_t network_status = NETWORK_FAILED;

    log_init(LOG_FILE_PATH);

    ASSERT_NOT_NULL(tool, cleanup, "tool was NULL");

    while(1)
    {
        network_status = communicate(tool, &sleep_duration, &should_die);
	
	if (0 != should_die || NETWORK_OK != network_status)
	{
		break;
	}

	if (0 == sleep_duration)
        {
            sleep(tool->conf.default_sleep);
        }
        else
        {
            sleep(sleep_duration);
        }
    } 

cleanup:
    log_destroy();
    return;
}

