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

#include "logc_server_utils.h"

#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#define SERVER_LOG_BUFF_SIZE 1024

/**
 * write log messages to server log file
 */
void server_log(int log_file_fd, char *file, int line, char *func, char *format, ...)
{
    char log_buff[SERVER_LOG_BUFF_SIZE];
    int n = snprintf(log_buff, SERVER_LOG_BUFF_SIZE, "%s | %d | %s | ", file, line, func);

    va_list va_args;
    va_start(va_args, format);
    n += vsnprintf(log_buff + n, SERVER_LOG_BUFF_SIZE - n, format, va_args);
    n += snprintf(log_buff + n, SERVER_LOG_BUFF_SIZE - n, "\n");

    write(log_file_fd, log_buff, n);
}

int
send_response(int fd, uint8_t *buffer, int len)
{
    int wb = write(fd, buffer, len);

    if(wb < 0)
        logc_server_log("Cannot send response to client. fd: %d, error: ", fd, strerror(errno));
    else
        logc_server_log("Response sent to client. fd: %d", fd);

    return wb;
}
