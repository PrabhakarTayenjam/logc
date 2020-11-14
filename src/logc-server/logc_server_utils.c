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

#include "logc_server.h"
#include "logc_server_utils.h"

#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/epoll.h>
#include <sys/mman.h>


#ifdef SERVER_LOG_ENABLE
void server_log(char *file, int line, char *func, char *format, ...)
{
    char log_buff[1024];
    int n = sprintf(log_buff, "%s | %d | %s | ", file, line, func);

    va_list va_args;
    va_start(va_args, format);
    n += vsprintf(log_buff + n, format, va_args);
    
    n += sprintf(log_buff + n, "\n");

    write(server_log_fd, log_buff, n);
    printf("%s", log_buff);
}
#endif

int
accept_conn_and_add_to_epoll(int listen_fd)
{
    struct sockaddr_un client_addr;
    
    int client_fd;
    unsigned int len;
    client_fd = accept(listen_fd, (struct sockaddr *)(&client_addr), &len);
    if(client_fd == -1) {
        logc_server_log("Cannot accept connection: %s", strerror(errno));
        return -1;
    }
    logc_server_log("Accepted connection from client. fd: %d", client_fd);

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = client_fd;
    int ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev);
    if(ret == -1) {
        logc_server_log("Cannot add to epoll: %s", strerror(errno));
        return -1;
    }
    logc_server_log("Added to epoll. client fd: %d", client_fd);
    
    return 0;
}

void close_client(int fd)
{
    /**
     * close and remove from epoll
     * closing a fd automatically removes the fd from epoll (if all the fd references are closed)
     */
    close(fd);
    // epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);

    // Get client details from  client info hash table
    struct logc_client_info *handle;
    handle = g_hash_table_lookup(client_info_ht, GINT_TO_POINTER(fd));
    if(handle == NULL) {
        logc_server_log("Client info not found. Already closed. client_fd: %d", fd);
        return;
    }

    // remove from hash table. This will free client info for fd
    g_hash_table_remove(client_info_ht, GINT_TO_POINTER(fd));

    // Check if another close process is in progress.
    // This may happen in case of client sending close request and terminating just after it
    int zero = 0;
    if(!__atomic_compare_exchange_n(&(handle->close_lock), &zero, 1, false, __ATOMIC_RELAXED, __ATOMIC_RELAXED))
    {
        logc_server_log("Already closed. client_fd: %d", fd);
        return;
    }

    // Read logs if there is any
    char read_buff[MAX_LOG_BUFF_SIZE];
    int n = logc_buffer_read_all(handle->log_buff, read_buff);

    // write to file
    if(n > 0) {
        fprintf(handle->fp, "%s", read_buff);
        logc_server_log("Written %d bytes to log file: %s", n, handle->log_file_path);
    }

    // Flush the log messages in buffer and close the fp
    fflush(handle->fp);
    fclose(handle->fp);

    // unmap memory
    munmap(handle->mmap_addr, MAX_LOG_BUFF_SIZE);

    logc_server_log("Client closed. fd = %d, log_file_path: %s", fd, handle->log_file_path);
}
