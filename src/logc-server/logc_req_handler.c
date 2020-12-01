/**
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

#include "logc_req_handler.h"
#include "logc_server.h"
#include "logc_server_utils.h"
#include "../common/logc_buffer.h"
#include "../common/logc_utils.h"

#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <unistd.h>


/**
 * Process init request 
 *
 * This functions will
 * Get the append mode and log file path from the request buffer 
 * Create a shared memory 
 * Open a file in the log file path 
 * Respond success / failure to the client 
 * 
 * @param c_info: information related to client
 * @param req_buffer: request buffer that was sent by the client
 * @return 0 on success, -1 on failure
 */
static int
process_init_req(struct client_info *c_info, uint8_t *req_buff)
{
    uint8_t success = 0;
    char shm_name[MAX_FILE_PATH_SIZE];

    /* parsing init request */

    uint8_t *ptr = req_buff + 1;  // skip 1 byte for request code
    c_info->append = *ptr;        // append mode
    ptr += 1; 
    strcpy(c_info->log_file_path, (char *)ptr); // log file path

    logc_server_log("Init request received. append_mode: %d, log_file_path: %s", c_info->append, c_info->log_file_path);

    // this loop will run once
    while(1) {
        // create a shared memory
        sprintf(shm_name, "/logc_shm_client_%d", c_info->fd);
        void *addr = create_shared_mem(shm_name);
        if(addr == NULL) {
            logc_server_log("Cannot create shared memory. shm_name: %s, error: %s", shm_name, strerror(errno));
            break;
        }

        // store shared memory addrress
        c_info->mmap_addr = addr;

        // map shared memory to log_buffer
        logc_buffer_map(c_info->log_buff, addr);

        // set open mode for the log file
        char *mode;
        if(c_info->append == 1)
            mode = "a";
        else
            mode = "w";

        // open the log file
        c_info->fp = fopen(c_info->log_file_path, mode);
        if(c_info->fp == NULL) {
            logc_server_log("Cannot open log file: %s, error: %s", c_info->log_file_path, strerror(errno));
            break;
        }

        // all completed successfully
        success = 1;
        break;
    }

    uint8_t resp_buff[MAX_WRITE_BUFF_SIZE];
    memcpy(resp_buff, &success, sizeof(uint8_t));

    if(success) {
        memcpy(resp_buff + 1, shm_name, strlen(shm_name) + 1);
        logc_server_log("Log init success");
    } else {
        memcpy(resp_buff + 1, &errno, sizeof(int));
        logc_server_log("Log init failed");
    }

    // Send response to client
    if(send_response(c_info->fd, resp_buff, MAX_WRITE_BUFF_SIZE) < 0)
        success = 0;

    if(success)
        return 0;
    return -1;
}

/**
 * Read from the logc_buff and write to the log file
 *
 * @param c_info: information related to client
 * @param req_buff: request buffer
 * @returns 0
 *
 * Note: This functions always succeeds
 */
static int
process_write_req(struct client_info *c_info, uint8_t *req_buff)
{
    logc_server_log("Received write request. fd: %d", c_info->fd);

    // Read logs
    char read_buff[MAX_LOG_BUFF_SIZE];
    int n_bytes = logc_buffer_read_all(c_info->log_buff, read_buff);
  
    if(n_bytes == 0) {
        logc_server_log("Nothing to write");
    }
    else {
        fprintf(c_info->fp, "%s", read_buff);
        fflush(c_info->fp);
        logc_server_log("Written %d bytes to log file: %s", n_bytes, c_info->log_file_path);
    }

    return 0;
}

/**
 * Close the logc client
 * Close all the related fds
 *
 * @param c_info: information related to the client
 * @param req_buff: request buffer
 *
 * @returns 1
 *
 * Note: This functions always succeeds
 */
static int
process_close_req(struct client_info *c_info, uint8_t *req_buff)
{
    logc_server_log("Received close request. fd: %d", c_info->fd);
    close_client(c_info);

    return 1;
}

void
close_client(struct client_info *c_info)
{
    // close the client connection
    close(c_info->fd);

    // close the client epoll fd
    close(c_info->epoll_fd);
   
    // read logs if there is any
    char read_buff[MAX_LOG_BUFF_SIZE];
    int n_bytes = logc_buffer_read_all(c_info->log_buff, read_buff);

    // write to file
    if(n_bytes > 0) {
        fprintf(c_info->fp, "%s", read_buff);
        logc_server_log("Written %d bytes to log file: %s", n_bytes, c_info->log_file_path);
    }

    // flush the log file fp and close it
    fflush(c_info->fp);
    fclose(c_info->fp);

    // unmap memory
    munmap(c_info->mmap_addr, MAX_LOG_BUFF_SIZE);

    logc_server_log("Client closed. fd = %d, log_file_path: %s", c_info->fd, c_info->log_file_path);
}

/**
 * Process client request
 *
 * @param c_info: information related to client
 * @param buffer: request buffer
 *
 * @returns 0 on success, -1 on failure, 1 on client closed
 */
int
process_client_request(struct client_info *c_info, uint8_t *buffer)
{
    // get the request type
    uint8_t req_type = *(uint8_t *)buffer;

    logc_server_log("Processing request. fd: %d, type: %d", c_info->fd, req_type);

    int ret;

    switch(req_type) {
    case REQUEST_INIT:
        ret = process_init_req(c_info, buffer);
        break;
    case REQUEST_WRITE:
        ret = process_write_req(c_info, buffer);
        break;
    case REQUEST_CLOSE:
        ret = process_close_req(c_info, buffer);
        break;
    default:
        logc_server_log("Invalid request received. fd: %d, type: %d", c_info->fd, req_type);
    }

    return ret;
}
