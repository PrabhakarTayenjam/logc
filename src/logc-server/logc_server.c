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
#include "logc_req_handler.h"
#include "logc_server_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <glib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/epoll.h>

#define LOGC_SERVER_SOCKET_PATH   "/dev/shm/logc.server"
#define LOGC_MAX_CLIENTS          10
#define EPOLL_MAX_EVENTS          10
#define EPOLL_TIMEOUT             3000

// Hash table to store client information
GHashTable *client_info_ht;

// Epoll interface
int epoll_fd;

// Server log fd for logging
int server_log_fd;

volatile int running = 1;

void *
client_thread(void *args)
{
    struct logc_client_info *client_info = (struct logc_client_info *)args;

    // Create epoll for client
    int client_epoll_fd = epoll_create(1);
    if(epoll_fd == -1) {
        logc_server_log("Cannot create epoll: %s", strerror(errno));
        free(args);
        return NULL;
    }

    // Add client fd to epoll
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = client_info->fd;
    
    int ret = epoll_ctl(client_epoll_fd, EPOLL_CTL_ADD, client_info->fd, &ev);
    if(ret == -1) {
        free(args);
        return NULL;
    }

    // Epoll wait
    int n_ready_events;
    struct epoll_event events[EPOLL_MAX_EVENTS];
    memset(events, 0, sizeof(struct epoll_event) * EPOLL_MAX_EVENTS);

    while(1) {
        n_ready_events = epoll_wait(client_epoll_fd, events, EPOLL_MAX_EVENTS, EPOLL_TIMEOUT);

        if(n_ready_events == 0)
            continue;

        for(int i = 0; i < n_ready_events; ++i) {
            if(events[i].events & (EPOLLERR | EPOLLHUP)) {
                // Error occured, exit thread
                logc_server_log("Client disconnected");
                close_client(client_info->fd);
                free(args);
                return NULL;
            }

            if(events[i].events & EPOLLIN) {
                // Request received from client
                logc_server_log("Request received from client: %d", client_info->fd);

                // Read
                uint8_t buffer[MAX_READ_BUFF_SIZE];
                
                int rb = read(client_info_fd, buffer, MAX_READ_BUFF_SIZE);
                if(rb <= 0) {
                    logc_server_log("Read failed. fd = %d, error: %s", client_info->fd, strerror(errno));
                    close_client(client_info->fd);
                    free(args);
                    return NULL;
                }

                // Process request
                process_request(client_info, buffer);
            }
        } // end of for loop
    }
}

static void
handle_client_connection(int listen_fd)
{
    struct sockaddr_un client_addr;
    int client_fd;
    unsigned int len;

    client_fd = accept(listen_fd, (struct sockaddr *)(&client_addr), &len);
    if(client_fd == -1) {
        logc_server_log("Cannot accept connection: %s", strerror(errno));
        return;
    }

    logc_server_log("Accepted connection from client. fd: %d", client_fd);

    struct logc_client_info *client_info = malloc(sizeof(struct logc_client_info));
    client_info.fd = client_fd;

    if(pthread_create(&client_info->tid, NULL, client_thread, client_info) != -) {
        logc_server_log("Cannot initialise request handler: %s", strerror(errno));
        return;
    }
}

void
sigint_handler(int sig)
{
    logc_server_log("SIGINT received");
    running = 0;
}

int
main(int argc, char **argv)
{
    signal(SIGINT, sigint_handler);

    int ret; // for checking return values

    // Server logs
    server_log_fd = open("logc_server.log", O_CREAT | O_WRONLY | O_TRUNC, 0666);
    if(server_log_fd == -1) {
        fprintf(stderr, "Logc-server start failed: Cannot create server log file, %s\n", strerror(errno));
    }

    // Init hash table
    client_info_ht = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, free);

    // Create epoll interface
    epoll_fd = epoll_create(1);
    if(epoll_fd == -1)
        exit_with_errno();

    // Create server listen fd
    int logc_server_fd;
    logc_server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if(logc_server_fd == -1)
        exit_with_errno();

    // Add server listen fd to epoll
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = logc_server_fd;
    ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, logc_server_fd, &ev);
    if(ret == -1)
        exit_with_errno();
    
    // Setup server address
    struct sockaddr_un server_addr;
    server_addr.sun_family = AF_UNIX;
    strcpy(server_addr.sun_path, LOGC_SERVER_SOCKET_PATH);

    // Bind the address
    unlink(LOGC_SERVER_SOCKET_PATH);
    ret = bind(logc_server_fd, (struct sockaddr *)(&server_addr), sizeof(server_addr));
    if(ret == -1)
        exit_with_errno();

    // Listen
    ret = listen(logc_server_fd, LOGC_MAX_CLIENTS);
    if(ret == -1)
        exit_with_errno();

    // Epoll wait
    int n_ready_events;
    struct epoll_event events[EPOLL_MAX_EVENTS];
    memset(events, 0, sizeof(struct epoll_event) * EPOLL_MAX_EVENTS);

    logc_server_log("Started logc server...");

    while(running) {
        n_ready_events = epoll_wait(epoll_fd, events, EPOLL_MAX_EVENTS, EPOLL_TIMEOUT);

        // If running != 1, epoll wait was interrupted
        if(n_ready_events < 0 && running == 1)
            continue; //exit_with_errno();

        if(n_ready_events == 0)
            continue;

        for(int i = 0; i < n_ready_events; ++i) {
            if(events[i].events & EPOLLERR || events[i].events & EPOLLHUP) {
                continue;
            }
            if(events[i].events & EPOLLIN) {
                // New client trying to connect.
                logc_server_lo("New connection request received");
                   
                // create a thread and handle client separately
                handle_client_connection(logc_server_fd);
            }
        }
    }

    close(epoll_fd);
    logc_server_log("Shuting down logc server");

    return 0;
}
