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
#include "logc_utils.h"
#include "../logc_buffer.h"

#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <unistd.h>
#include <glib.h>


// Request codes
#define REQUEST_INIT            1
#define REQUEST_WRITE           2
#define REQUEST_CLOSE           3



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
        logc_server_log("Cannot create shared memory file: %s, error: %s", name, strerror(errno));
        return NULL;
    }

    ftruncate(fd, MAX_LOG_BUFF_SIZE);

    void *addr = mmap(NULL, MAX_LOG_BUFF_SIZE, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);
    if(addr == MAP_FAILED) {
        logc_server_log("Cannot create memory map. name: %s, error: %s", name, strerror(errno));
        return NULL;
    }

    close(fd);
    return addr;
}

/**
 * Process init request 
 * Get the append mode and log file path from the request buffer 
 * Create a shared memory 
 * Open a file in the log file path 
 * Add the info to a hash table with fd as the key 
 * Respond success / failure to the client 
 * 
 * @param client_fd fd of the client
 * @param req_buffer request buffer that was sent by the client
 */
void
process_init_req(int client_fd, uint8_t *req_buff)
{
    uint8_t success = 1;
    char shm_name[MAX_FILE_PATH_SIZE];

    logc_server_log("Recieved init request. fd = %d", client_fd);

    struct logc_client_info *handle =
        (struct logc_client_info *) malloc(sizeof(struct logc_client_info));

    // Parse request
    uint8_t *ptr = req_buff + 1; // +1 for request code
    handle->append = *ptr;
    ptr += 1; 
    strcpy(handle->log_file_path, (char *)ptr);

    logc_server_log("append: %d, log_file_path: %s", handle->append, handle->log_file_path);

    while(1) {
        // Create a shared memory
        sprintf(shm_name, "/logc_shm_client_%d", client_fd);
        void *addr = create_shared_mem(shm_name);
        if(addr == NULL) {
            logc_server_log("Cannot create shared memory");
            success = 0;
            break;
        }
        handle->mmap_addr = addr;

        // map shared memory to log_buffer
        handle->log_buff = (char *)addr;

        // Open file
        char *mode;
        if(handle->append == 1) mode = "a"; else mode = "w";
        handle->fp = fopen(handle->log_file_path, mode);
        if(handle->fp == NULL) {
            logc_server_log("Cannot open log file: %s, error: %s", handle->log_file_path, strerror(errno));
            success = 0;
            break;
        }

        // Add the info in hash table
        if(!g_hash_table_insert(client_info_ht, GINT_TO_POINTER(client_fd), handle)) {
            logc_server_log("Cannot insert client info to hash table");
            success = 0;
            break;
        }
        break;
    }

    uint8_t *resp_buff[MAX_WRITE_BUFF_SIZE];
    memcpy(resp_buff, &success, sizeof(uint8_t));
    if(success) {
        memcpy(resp_buff + 1, shm_name, strlen(shm_name) + 1);
        logc_server_log("Log init success");
    } else {
        logc_server_log("Log init failed");
    }

    // Send response to client
    if(write(client_fd, shm_name, strlen(shm_name)) <= 0)
        logc_server_log("Cannot sent init response to client. fd: %d", client_fd);
    else
        logc_server_log("Init response sent to client. fd: %d", client_fd);
}

void
process_write_req(int client_fd, uint8_t *req_buff)
{
    logc_server_log("Received write request. fd: %d", client_fd);

    struct logc_client_info *handle = g_hash_table_lookup(client_info_ht, GINT_TO_POINTER(client_fd));
    if(handle == NULL) {
        logc_server_log("Write failed. Client info not found. fd: %d", client_fd);
        return;
    }

    int *r_offset;
    int *w_offset;
    int *marker;

    void *ptr = req_buff + 1; // +1 for code
    r_offset = (int *)ptr;
    ptr += sizeof(int);
    w_offset = (int *)ptr;
    ptr += sizeof(int);
    marker = (int *)ptr;

    // Read logs
    int n;
    char read_buff[MAX_LOG_BUFF_SIZE];
    if(*r_offset < *w_offset) {
        n = *w_offset - *r_offset;
        memcpy(read_buff, handle->log_buff + *r_offset, n);
        read_buff[n] = '\0';
    }
    else {
        int n = *marker - *r_offset;
        memcpy(read_buff, handle->log_buff + *r_offset, n);
        memcpy(read_buff + n, handle->log_buff, *w_offset);
        n += *w_offset;
        read_buff[n] = '\0';
    }

    fprintf(handle->fp, "%s", read_buff);
    fflush(handle->fp);

    logc_server_log("Written %d bytes to log file: %s", n, handle->log_file_path);
}

void
process_close_req(int client_fd, uint8_t *req_buff)
{
    logc_server_log("Received close request. fd: %d", client_fd);
    close_client(client_fd);
}

void *
process_req(void *args)
{
    struct logc_request *req = (struct logc_request *)args;
    int fd = req->fd;
    uint8_t *buffer = req->buffer;

    logc_server_log("Processing request. fd = %d", fd);    

    uint8_t *req_type = (uint8_t *)buffer;

    logc_server_log("Received request. type: %d", *req_type);

    switch(*req_type) {
    case REQUEST_INIT:
        process_init_req(fd, buffer);
        break;
    case REQUEST_WRITE:
        process_write_req(fd, buffer);
        break;
    case REQUEST_CLOSE:
        process_close_req(fd, buffer);
        break;
    default:
        logc_server_log("Invalid request received: %d", *req_type);
    }

    return NULL;
}