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
#include "../common/logc_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/epoll.h>

#define LOGC_MAX_CLIENTS          10
#define EPOLL_MAX_EVENTS          LOGC_MAX_CLIENTS
#define EPOLL_TIMEOUT             1000


// logc server log fd for logging
int server_log_fd;

// if 1, server logging will be done
int server_log_enable = 1;    

// logc server listen fd
int logc_listen_fd;

// logc server epoll fd
int logc_epoll_fd;

volatile int running = 1;


void
sigint_handler(int sig)
{
    logc_server_log("SIGINT received");
    running = 0;
}

/**
 * Initialise logc server
 * This function will
 *    open the logc server_log_fd
 *    open the logc_logc_listen_fd
 *    create the logc_logc_epoll_fd
 *    add the logc_listen fd to the logc_logc_epoll_fd
 *    start listening for connections on logc_logc_listen_fd
 */
static void
logc_server_init()
{
    int ret; // for checking return values

    // open server log file
    server_log_fd = open("logc_server.log", O_CREAT | O_WRONLY | O_TRUNC, 0666);
    if(server_log_fd == -1)
        exit_with_errno();

    // create server listen fd
    logc_listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if(logc_listen_fd == -1)
        exit_with_errno();
   
    // setup server address
    struct sockaddr_un server_addr;
    server_addr.sun_family = AF_UNIX;
    strcpy(server_addr.sun_path, LOGC_SERVER_SOCKET_PATH);

    // bind the address
    unlink(LOGC_SERVER_SOCKET_PATH);
    ret = bind(logc_listen_fd, (struct sockaddr *)(&server_addr), sizeof(server_addr));
    if(ret == -1)
        exit_with_errno();

    /**
    * create epoll interface
    * this epoll interface will be used for listening for new connections to the logc server
    */
    logc_epoll_fd = epoll_create(1);
    if(logc_epoll_fd == -1)
        exit_with_errno();

    // add server listen fd to epoll
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = logc_listen_fd;
    ret = epoll_ctl(logc_epoll_fd, EPOLL_CTL_ADD, logc_listen_fd, &ev);
    if(ret == -1)
        exit_with_errno();
 
    // listen
    ret = listen(logc_listen_fd, LOGC_MAX_CLIENTS);
    if(ret == -1)
        exit_with_errno();

    logc_server_log("Started logc server...");
}

/**
 * close the logc server
 * close all the fds
 */
static void
logc_server_close()
{
  logc_server_log("Shuting down logc server");
  
  close(server_log_fd);
  close(logc_epoll_fd);
  close(logc_listen_fd);
}

/**
 * free client_info
 */
static void
client_info_free(struct client_info *c_info)
{
    free(c_info);
}

/**
 * logc server client handler thread
 * all the request/response for a client will be done by this thread
 */
static void *
client_thread(void *args)
{
    struct client_info *c_info = (struct client_info *)args;

    // return value for process request
    int proc_req_ret = 0;
    
    // buffer for reading request
    uint8_t read_buffer[MAX_READ_BUFF_SIZE];

    // create epoll interface for the client
    c_info->epoll_fd = epoll_create(1);

    if(c_info->epoll_fd != -1) {
        // add the client fd to client epoll
        struct epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.fd = c_info->fd;
        int ret = epoll_ctl(c_info->epoll_fd, EPOLL_CTL_ADD, c_info->fd, &ev);

        if(ret != -1) {
            int n_ready_events;
            struct epoll_event events[EPOLL_MAX_EVENTS];
            memset(events, 0, sizeof(struct epoll_event) * EPOLL_MAX_EVENTS);

            // client loop
            int client_connected = 1;
            while(client_connected) {
                // wait for events
                n_ready_events = epoll_wait(c_info->epoll_fd, events, EPOLL_MAX_EVENTS, EPOLL_TIMEOUT);

                // epoll wait failed. exit client thread
                if(n_ready_events < 0) {
                    logc_server_log("epoll wait failed: %s", strerror(errno));
                    break;
                }
                
                for(int i = 0; i < n_ready_events; ++i) {
                    /**
                     * process events
                     */

                    if(events[i].events & EPOLLERR) {
                        /**
                         * client closes the connection
                         * break the client loop
                         */

                        logc_server_log("Connection closed. fd: %d", c_info->fd);
                        client_connected = 0;
                        continue;
                    }
                    else if(events[i].events & EPOLLIN) {
                        /**
                         * request received
                         * read the request and process
                         */

                        logc_server_log("Request received from client: %d", c_info->fd);

                        // read
                        int rb = read(c_info->fd, read_buffer, MAX_READ_BUFF_SIZE);
                        if(rb <= 0) {
                            /**
                             * read failed
                             * client closes the connection or some error occured
                             */

                            if(rb == 0)
                                logc_server_log("Connection closed. fd: %d", c_info->fd);
                            else
                                logc_server_log("Read failed. fd: %d, error: %s", c_info->fd, strerror(errno));
                        
                            client_connected = 0;
                            continue;
                        }
                        else {
                            // process the request
                            proc_req_ret = process_client_request(c_info, read_buffer);
                            
                            /**
                             * if ret ==  0, success
                             * if ret ==  1, client has been closed as close request is recieved
                             * if ret == -1, some error occured
                             */
                            if(proc_req_ret == -1 || proc_req_ret == 1)
                                client_connected = 0;   // break the loop
                        }
                    }
                } // end of for
            } // end of while
        }
        else {
            logc_server_log("Cannot add to epoll: %s", strerror(errno));
        }
    }
    else {
        logc_server_log("Cannot create epoll interface: %s", strerror(errno));
    }

    // close the client if it was not closed properly
    if(proc_req_ret != 1)
        close_client(c_info);

    // cleanup client info structure
    client_info_free(c_info);

    return NULL;
}

/**
 * logc server main loop
 * this function will initialise the logc server and wait for connections
 * if a client connects, it will create a separate client thread
 * and the handling of the client will be done by the client thread
 * when the logc server main loop ends, it will close the logc server
 */
void
logc_server_main()
{
    signal(SIGINT, sigint_handler);

    // initialise logc server
    logc_server_init();

    // for epoll wait
    int n_ready_events;
    struct epoll_event events[EPOLL_MAX_EVENTS];
    memset(events, 0, sizeof(struct epoll_event) * EPOLL_MAX_EVENTS);

    while(running) {
        n_ready_events = epoll_wait(logc_epoll_fd, events, EPOLL_MAX_EVENTS, EPOLL_TIMEOUT);

        // epoll wait error
        if(n_ready_events < 0) {
            // do not log if epoll wait was interrupted
            if(errno != EINTR)
                logc_server_log("epoll wait error: %s", strerror(errno));

            // break loop, shut down logc server
            break;
        }

        for(int i = 0; i < n_ready_events; ++i) {
            /**
             * process events on the logc_listen_fd
             */

            if((events[i].events & EPOLLIN)) {
                /**
                 * new client connection
                 * accept connection and start client thread
                 */

                struct client_info *c_info = (struct client_info *)malloc(sizeof(struct client_info));

                // accept connection
                socklen_t sock_addr_len;
                struct sockaddr_un client_sock_addr;

                c_info->fd = accept(logc_listen_fd, (struct sockaddr *)&client_sock_addr, &sock_addr_len);
                if(c_info->fd == -1) {
                    logc_server_log("Cannot accept new client connection: %s", strerror(errno));
                    continue;
                }

                logc_server_log("Client connected. fd: %d", c_info->fd);

                // start client thread
                pthread_create(&(c_info->tid), NULL, client_thread, c_info);
            }
        } // end of for
    } // end of while

    // shit down logc server
    logc_server_close();
}

int
main(int argc, char **argv)
{
    logc_server_main();
    return 0;
}
