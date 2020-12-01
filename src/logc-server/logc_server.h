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

#ifndef LOGC_SERVER_H
#define LOGC_SERVER_H

#include "../common/logc_buffer.h"
#include "../common/logc_utils.h"

#include <stdio.h>        // for FILE
#include <pthread.h>      // for pthread_t


struct client_info
{
    /* connection fd with the client*/
    int fd;

    /* epoll fd for the client */
    int epoll_fd;

    /* thread id for the client thread */
    pthread_t tid;

    /* append mode of the log file */
    int  append;

    /* absolute path of the log file for the client */
    char log_file_path[MAX_FILE_PATH_SIZE];

    /* wait free ring buffer for storing log messages */
    struct logc_buffer *log_buff;

    /* file pointer for the log file */
    FILE *fp;

    /* start address of the memory mapped address */
    void *mmap_addr;
};

#endif
