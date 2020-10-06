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
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/epoll.h>

#define LOGC_SERVER_SOCKET_PATH "/dev/shm/logc.server"
#define LOGC_MAX_CLIENTS 10
#define EPOLL_MAX_EVENTS LOGC_MAX_CLIENTS
#define EPOLL_TIMEOUT 1000

// Hash table to store client information
GHashTable *client_info_ht;

// Epoll interface
int epoll_fd;

volatile int running = 1;

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

    // Init hash table
    client_info_ht = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, free);

    // Create epoll interface
    epoll_fd = epoll_create(1);
    if(epoll_fd == -1)
        exit_with_errno();

    // Create server listen fd
    int listen_fd;
    listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if(listen_fd == -1)
        exit_with_errno();

    // Add server listen fd to epoll
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = listen_fd;
    ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &ev);
    if(ret == -1)
        exit_with_errno();
    
    // Setup server address
    struct sockaddr_un server_addr;
    server_addr.sun_family = AF_UNIX;
    strcpy(server_addr.sun_path, LOGC_SERVER_SOCKET_PATH);

    // Bind the address
    unlink(LOGC_SERVER_SOCKET_PATH);
    ret = bind(listen_fd, (struct sockaddr *)(&server_addr), sizeof(server_addr));
    if(ret == -1)
        exit_with_errno();

    // Listen
    ret = listen(listen_fd, LOGC_MAX_CLIENTS);
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
            exit_with_errno();

        if(n_ready_events == 0)
            continue;

        for(int i = 0; i < n_ready_events; ++i) {
            if(events[i].events & EPOLLERR || events[i].events & EPOLLHUP) {
                logc_server_log("Client disconnected. fd: %d", events[i].data.fd);
                close_client(events[i].data.fd);
                continue;
            }
            if(events[i].events & EPOLLIN) {
                if(events[i].data.fd == listen_fd) {
                    // New client trying to connect.
                    logc_server_log("New connection request received");
                    
                    // accept connection and add to epoll
                    accept_conn_and_add_to_epoll(listen_fd);
                } else {
                    // Request recieved from clients
                    int fd = events[i].data.fd;
                    logc_server_log("Request received from client: %d", fd);

                    // Read
                    uint8_t buffer[MAX_READ_BUFF_SIZE];
                    int rb;

                    rb = read(fd, buffer, MAX_READ_BUFF_SIZE);
                    if(rb <= 0) {
                        if(rb == 0)
                            logc_server_log("Connection closed. fd = %d", fd);
                        else
                            logc_server_log("Cannot read request. fd = %d, error: %s", fd, strerror(errno));
                        
                        close_client(fd);
                        continue;
                    }

                    struct logc_request req;
                    req.fd = fd;
                    req.buffer = buffer;

                    pthread_t tid;
                    ret = pthread_create(&tid, NULL, process_req, &req);
                    if(ret == -1)
                        logc_server_log("Cannot process request. fd = %d", fd);
                }
            }
        }
    }

    close(epoll_fd);
    logc_server_log("Shuting down logc server");

    return 0;
}