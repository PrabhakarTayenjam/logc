/*
 * MIT License
 * 
 * Copyright (c) 2020 Stardust
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "logc.h"
#include "../common/logc_utils.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <time.h>

#define LOGC_SERVER_SOCKET_PATH "/dev/shm/logc.server"
#define REQ_BUFF_SIZE 128
#define RESP_BUFF_SIZE 128


/**
 * Log msg format
 * date time | file | func | line | msg
 */
void write_log_to_buffer__(struct logc_handle *handle, char * file, char *func, int line, const char *format, ...)
{
    va_list va_args;
    char buff[1024];
    int len = 0;

    // fill date time
    time_t cur_time = time(NULL);
    len += strftime(buff, 1024, "%c", localtime(&cur_time));
    len += sprintf(buff + len, " | %s | %s | %d | ", file, func, line);

    va_start(va_args, format);
    len += vsnprintf(buff + len, 1024, format, va_args);

    // add end line
    buff[len] = '\n';
    len++;

    // Write to logc_buffer
    if(logc_buffer_write(handle->log_buffer, buff, len) == 1) {
        // Threshold reached send write request
        send_write_request(handle);
    }
}

static int
send_init_request(struct logc_handle *handle)
{
    int log_file_path_len = strlen(handle->log_file_path) + 1;
    // Make request
    uint8_t code = REQUEST_INIT;
    uint8_t req_buff[REQ_BUFF_SIZE];

    memcpy(req_buff, &code, sizeof(uint8_t));
    memcpy(req_buff + 1, &(handle->append), sizeof(uint8_t));
    memcpy(req_buff + 2, handle->log_file_path, log_file_path_len);

    int sz = 2 + log_file_path_len;
    // Send
    int wb = write(handle->fd, req_buff, sz);
    if(wb <= 0)
        return -1;

    return 0;
}

static int
wait_for_init_response(struct logc_handle *handle, char *shm_name)
{
    uint8_t resp_buff[RESP_BUFF_SIZE];
    int rb = read(handle->fd, resp_buff, RESP_BUFF_SIZE);
    if(rb <= 0)
        return -1;

    // Failed in server side, will get errno
    if(*((uint8_t *)resp_buff) == 0) {
        errno = *(int *)(resp_buff + 1);
        return -1;
    }

    strcpy(shm_name, (char *)(resp_buff + 1));
    return 0;
}

#ifdef LOGC_DEBUG
int
#else
static int
#endif
send_write_request(struct logc_handle *handle)
{
    // Make write request
    uint8_t req_buff[REQ_BUFF_SIZE];
    uint8_t code = REQUEST_WRITE;

    uint8_t *ptr = req_buff;
    memcpy(ptr, &code, sizeof(uint8_t));

    if(write(handle->fd, &req_buff, 1) <= 0)
        return -1;
    return 0;
}

struct logc_handle *
logc_handle_init(char *log_file_path, enum logc_level level, bool append)
{
    struct logc_handle *handle = (struct logc_handle *)malloc(sizeof(struct logc_handle));

    strcpy(handle->log_file_path, log_file_path);
    handle->level = level;
    handle->append = append;

    return handle;
}

int
logc_connect(struct logc_handle *handle)
{
    handle->fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if(handle->fd == -1)
        return -1;

    struct sockaddr_un logc_server_addr;
    logc_server_addr.sun_family = AF_UNIX;
    strcpy(logc_server_addr.sun_path, LOGC_SERVER_SOCKET_PATH);

    int ret;

    // Connect to logc server
    ret = connect(handle->fd, (struct sockaddr *)(&logc_server_addr), sizeof(logc_server_addr));
    if(ret == -1) {
        console_log("error: %s\n\n", strerror(errno));
        return -1;
    }

    // Send init request
    ret = send_init_request(handle);
    if(ret == -1)
        return -1;

    // Wait for response
    char shm_name[MAX_FILE_PATH_SIZE];
    ret = wait_for_init_response(handle, shm_name);
    if(ret != 0) {
        return -1;
    }

    // Create shared memory
    void *addr = create_shared_mem(shm_name);
    if(addr == NULL) {
        return -1;
    }

    // Map logc_buffer with shared memory
    logc_buffer_map_and_init(handle->log_buffer, addr);

    return 0;
}

int
logc_close(struct logc_handle *handle)
{
    // Make request
    uint8_t code = REQUEST_CLOSE;
    uint8_t req_buff[REQ_BUFF_SIZE];

    memcpy(req_buff, &code, 1);

    if( (write(handle->fd, req_buff, 1)) <= 0)
        return -1;

    // close(handle->fd);
    return 0;
}
