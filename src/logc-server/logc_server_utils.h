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

#ifndef LOGC_SERVER_UTILS
#define LOGC_SERVER_UTILS

#include <stdio.h>
#include <stdint.h>


extern int server_log_enable;
extern int server_log_fd;


void server_log(int log_file_fd, char *file, int line, char *function, char *format, ...);

#define logc_server_log(...) do\
{ \
    if(server_log_enable) \
        server_log(server_log_fd, __FILE__, __LINE__, (char *)__func__, __VA_ARGS__); \
} while(0)

#define exit_with_errno() do\
{ \
    fprintf(stderr, "Logc server error: %s\n", strerror(errno)); \
    exit(1); \
} while(0)

/**
 * Send response to a client
 *
 * @param fd: connection fd to the client
 * @param buffer: message buffer
 * @param len: len of the message buffer to be sent
 *
 * @returns size in bytes sent to the client, -1 on failure
 */
int send_response(int fd, uint8_t *buffer, int len);

#endif
