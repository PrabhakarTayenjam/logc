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

#include "logc_utils.h"

#include <sys/mman.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>

#ifdef LOGC_DEBUG
void
console_log__(char *file, char *function, int line, char *format, ...)
{
    char log_buff[512];
    int n = 0;

    n += sprintf(log_buff, "%s | %s | %d | ", file, function, line);

    va_list va_args;
    va_start(va_args, format);
    vsprintf(log_buff + n, format, va_args);

    printf("%s\n", log_buff);
}
#endif

void *
create_shared_mem(char *name)
{
    int fd = shm_open(name, O_CREAT | O_RDWR, 0666);
    if(fd == -1) {
        return NULL;
    }

    ftruncate(fd, MAX_LOG_BUFF_SIZE);

    void *addr = mmap(NULL, MAX_LOG_BUFF_SIZE, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);
    if(addr == MAP_FAILED) {
        return NULL;
    }

    close(fd);
    return addr;
}