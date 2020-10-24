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

/**
 * Logc wait free buffer
 * Implementation of wait free atomic ring buffer
 */

#ifndef LOGC_BUFFER_H
#define LOGC_BUFFER_H

#include <stdint.h>

struct logc_buffer
{
    uint32_t w_offset;      // write offset
    uint32_t r_offset;      // read offset
    uint32_t marker;        // offset at the end of buffer upto which data is written before wrap around
    uint32_t size;          // size of the buffer in bytes
    uint32_t used;          // total bytes used in buffer
    uint32_t threshold;     // threshold in bytes
    uint32_t read_lock;     // use as lock for reading
    uint32_t hole;          // for alignment
    char     buffer[];      // logging buffer
};

/**
 * This will map the shared memory to the logc_buffer structure
 * 
 * @param handle A logc handle
 * @param addr A shared momory address
 */
#define logc_buffer_map(handle, addr) (handle) = (struct logc_buffer *)(addr);

/**
 * This will map the shared memory to the logc_buffer structure 
 * And initialise the logc_buffer structure
 * 
 * @param handle A logc handle
 * @param addr A shared momory address
 */
#define logc_buffer_map_and_init(handle, addr) \
{ \
    (handle) = (struct logc_buffer *)(addr); \
    (handle)->w_offset = 0; \
    (handle)->r_offset = 0; \
    (handle)->marker = 0; \
    (handle)->size = MAX_LOG_BUFF_SIZE; \
    (handle)->used = 0; \
    (handle)->threshold = MAX_LOG_BUFF_SIZE * 0.5; \
    (handle)->read_lock = 0; \
}

/**
 * Write msg to logc buffer
 * 
 * @param handle A logc_buffer handle
 * @param msg Pointer to the msg
 * @param len Length of the msg
 * 
 * @returns 1 if threshold of the logc_buffer is reached, 0 otherwise
 */
int logc_buffer_write(struct logc_buffer *handle, char *msg, int len);

/**
 * Read all the messages from logc buffer to read_buff
 * This will read all the messages from r_offset to w_offset
 * 
 * @param handle A logc_buffer handle
 * @param read_buff A pointer to a buffer. The size of this buffer should at least equal to threshold
 * 
 * @return Size read in bytes. 0 if nothing was read
 */
int logc_buffer_read_all(struct logc_buffer *handle, char *read_buff);

#endif