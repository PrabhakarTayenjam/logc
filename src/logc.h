/*
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

#ifndef LOGC_H
#define LOGC_H

#include "logc_buffer.h"

#include <assert.h>
#include <stddef.h>
#include <stdbool.h>


#define MAX_FILE_PATH_SIZE 128

enum logc_level
{
    ALL, INFO, DEBUG, WARN, ERROR, TRACE, DISABLE
};

struct logc_handle
{
    char log_file_path[MAX_FILE_PATH_SIZE];
    enum logc_level level;
    uint8_t  append;
    struct logc_buffer log_buffer;
    int fd;
};


/**
 * write_log_to_buffer__
 * 
 * Builds the log message and write it to the logc_buffer
 * If the usage threshold of the logc_buffer is reached,
 * Sends write request to logc server
 * 
 * @param handle Log handle
 * @param file current file
 * @param func current function
 * @param line current line
 * @param format format of the log message
 */
void write_log_to_buffer__(struct logc_handle *handle, char * file, char *func, int line, const char *format, ...);


/**
 * logc_log
 * Writes the log to logc_buffer if log_level is greater than or equal
 * to the log level of the handle.
 * 
 * @note Do not use this macro to write logs. Use the macros log_info, log_debug, log_warn, log_error, log_trace
 * 
 * @param handle: A logger handle
 * @param log_level: Log level
 **/
#define logc_log(handle, log_level, ...) \
{ \
    assert((handle) != NULL); \
    if(log_level >= (handle)->level) \
        write_log_to_buffer__(handle, __FILE__, (char *)__func__, __LINE__, __VA_ARGS__); \
}

/**
 * log_info
 * Write info logs if the level of the handle is less than or equal to INFO
 */
#define log_info(handle, ...) logc_log(handle, INFO, __VA_ARGS__)

/**
 * log_debug
 * Write debug logs if the level of the handle is less than or equal to DEBUG
 */
#define logc_debug(handle, ...) logc_log(handle, DEBUG, __VA_ARGS__)

/**
 * log_warn
 * Write warning logs if the level of the handle is less than or equal to WARN
 */
#define logc_warn(handle, ...) logc_log(handle, WARN, __VA_ARGS__)

/**
 * log_error
 * Write error logs if the level of the handle is less than or equal to ERROR
 */
#define logc_error(handle, ...) logc_log(handle, ERROR, __VA_ARGS__)

/**
 * log_trace
 * Write trace logs if the level of the handle is less than or equal to TRACE
 */
#define logc_trace(handle, ...) logc_log(handle, TRACE, __VA_ARGS__)


/**
 * Create a logc handle with the given information
 * 
 * @param log_file_path Full path of the log file
 * @param level Log level
 * @param append If 1, log file will be open in append mode
 * 
 * @return A logc handle
 */
struct logc_handle * logc_handle_init(char *log_file_path, enum logc_level level, bool append);

/**
 * Connect to the logc server. 
 * This will send the log init request and setup the logger in logc server
 * If the process fails at the server side, server will send errno in the response
 * 
 * @param handle A logc handle
 * 
 * @return 0 if success, -1 if failed. The error can check with errno
 */
int logc_connect(struct logc_handle *handle);


#endif