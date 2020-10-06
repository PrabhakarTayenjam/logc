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

#ifndef LOGC_UTILS
#define LOGC_UTILS

#include "logc_server.h"

#include <stdio.h>
#include <sys/epoll.h>


#define SERVER_LOG_ENABLE 1

void server_log(char *file, int line, char *function, char *format, ...);

#ifdef SERVER_LOG_ENABLE
#define logc_server_log(...) \
    server_log(__FILE__, __LINE__, (char *)__func__, __VA_ARGS__)
#else
#define logc_server_log(...) {}
#endif

#define exit_with_errno() \
{ \
    logc_server_log("Logc server error: %s\n", strerror(errno)); \
    exit(1); \
}

/**
 * Accept the connection from listen_fd and add the fd to epoll_fd
 * 
 * @param epoll_fd fd for the epoll interface
 * @param listen_fd Logc server listen fd
 * @return -1 if failed, 0 is success
 **/
int accept_conn_and_add_to_epoll(int listen_fd);

/**
 * Close the connection with the client and cleanup its realated informations
 * 
 * @param fd File descriptor of the client
 */
void close_client(int fd);

#endif