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

#ifndef LOGC_UTILS_H
#define LOGC_UTILS_H



#define LOGC_SERVER_SOCKET_PATH   "/dev/shm/logc.server"
#define MAX_LOG_BUFF_SIZE     1024 * 16
#define MAX_READ_BUFF_SIZE    128
#define MAX_WRITE_BUFF_SIZE   128
#define MAX_FILE_PATH_SIZE    128

// Request codes
#define REQUEST_INIT            1
#define REQUEST_WRITE           2
#define REQUEST_CLOSE           3


#ifdef LOGC_DEBUG
    void console_log__(char *file, char *function, int line, char *format, ...);

    #define console_log(...) console_log__(__FILE__, (char *)__func__, __LINE__, __VA_ARGS__);
#else
    #define console_log(...)
#endif


/**
 * Create a shared memory with the given name in read write mode.
 * And map the file to memory.
 * 
 * @param name name of the shared memory file
 * @return address of mapped memory, NULL if failed
 */
void *create_shared_mem(char *name);

#endif
