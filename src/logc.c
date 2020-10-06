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
#define MAX_LOG_BUFF_SIZE 1024 * 16

// Request codes
#define REQUEST_INIT            1
#define REQUEST_WRITE           2
#define REQUEST_CLOSE           3

int send_write = 0;

static int
send_init_request(struct logc_handle *handle)
{
    // Make request
    uint8_t code = REQUEST_INIT;
    uint8_t req_buff[REQ_BUFF_SIZE];

    memcpy(req_buff, &code, sizeof(uint8_t));
    memcpy(req_buff + 1, &(handle->append), sizeof(uint8_t));
    memcpy(req_buff + 2, handle->log_file_path, strlen(handle->log_file_path) + 1);

    // Send
    int wb = write(handle->fd, req_buff, REQ_BUFF_SIZE);
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
    if(*resp_buff == 0) {
        errno = *(int *)(resp_buff + 1);
        return -1;
    }

    strcpy(shm_name, (char *)(resp_buff + 1));
    return 0;
}

static int
send_write_request(struct logc_handle *handle)
{
    int r_offset;
    int w_offset;
    int marker;

    // Get the offsets
    if(! logc_buffer_read(&(handle->log_buffer), &r_offset, &w_offset, &marker)) // already read
        return 1;

    // Make write request
    uint8_t req_buff[REQ_BUFF_SIZE];
    uint8_t code = REQUEST_WRITE;

    uint8_t *ptr = req_buff;
    memcpy(ptr, &code, sizeof(uint8_t));
    ptr += sizeof(uint8_t);
    memcpy(ptr, &r_offset, sizeof(int));
    ptr += sizeof(int);
    memcpy(ptr, &w_offset, sizeof(int));
    ptr += sizeof(int);
    memcpy(ptr, &marker, sizeof(int));


    if(write(handle->fd, &req_buff, sizeof(uint8_t) + 3 * sizeof(int)) <= 0)
        return -1;
    return 0;
}

/**
 * Create a shared memory with the given name in read write mode.
 * And map the file to memory.
 * 
 * @param name name of the shared memory file
 * @return address of mapped memory
 */
static void *
create_shared_mem(char *name)
{
    int fd = shm_open(name, O_CREAT | O_RDWR, 0666);
    if(fd == -1) {
        return NULL;
    }

    ftruncate(fd, MAX_LOG_BUFF_SIZE);

    void *addr = mmap(NULL, MAX_LOG_BUFF_SIZE, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);
    if(addr == MAP_FAILED) {
        return NULL;
    }

    close(fd);
    return addr;
}

static inline int
get_cur_time(char *buff, size_t sz)
{
    time_t cur_time = time(NULL);
    int n = strftime(buff, sz, "%c", localtime(&cur_time));
    return n;
}

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
    len += get_cur_time(buff, 1024);
    len += sprintf(buff + len, " | file: %s | func: %s | line: %d | ", file, func, line);

    va_start(va_args, format);
    len += vsnprintf(buff + len, 1024, format, va_args);

    // Write to logc_buffer
    if(logc_buffer_write(&(handle->log_buffer), buff, len) == 1) {
        // Threshold reached send write request
        send_write_request(handle);
    }

    printf("%s\n", buff);
}

struct logc_handle *
logc_handle_init(char *log_file_path, enum logc_level level, bool append)
{
    struct logc_handle *handle = (struct logc_handle *)malloc(sizeof(struct logc_handle));

    strcpy(handle->log_file_path, log_file_path);
    handle->level = level;
    handle->append = append;

    // logc_buffer
    memset(&(handle->log_buffer), 0, sizeof(struct logc_buffer));
    handle->log_buffer.threshold = MAX_LOG_BUFF_SIZE * 0.5; // 50 % threshold

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
    if(ret == -1)
        return -1;

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

    // Map logc_buffer with shared memory
    handle->log_buffer.buffer = (char *)addr;

    return 0;
}