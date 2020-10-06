#include "logc_utils.h"

#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/mman.h>


void server_log(char *file, int line, char *func, char *format, ...)
{
    char log_buff[1024];
    int n = sprintf(log_buff, "%s | %d | %s | ", file, line, func);

    va_list va_args;
    va_start(va_args, format);
    vsnprintf(log_buff + n, 1024, format, va_args);

    fprintf(stdout, "%s\n", log_buff);
}

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

void
close_client(int fd)
{
    /**
     * close and remove from epoll
     * closing a fd automatically removes the fd from epoll (if all the fd references are closed)
     */
    // epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
    close(fd);

    // Get client details from  client info hash table
    struct logc_client_info *handle;
    handle = g_hash_table_lookup(client_info_ht, GINT_TO_POINTER(fd));
    if(handle == NULL) {
        logc_server_log("Client info not found. client_fd: %d", fd);
        return;
    }

    // Flush the log messages in buffer and close the fp
    fflush(handle->fp);
    fclose(handle->fp);

    // unmap memory
    munmap(handle->mmap_addr, MAX_LOG_BUFF_SIZE);

    logc_server_log("Client closed. fd = %d, log_file_path: %s", fd, handle->log_file_path);

    // remove from hash table. This will free client info for fd
    g_hash_table_remove(client_info_ht, GINT_TO_POINTER(fd));
}