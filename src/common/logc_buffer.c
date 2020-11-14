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

#include "logc_buffer.h"

#include <string.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef LOGC_DEBUG
#include "logc_utils.h"
#endif

int
logc_buffer_write(struct logc_buffer *handle, char *msg, int len)
{
    int l_write = __atomic_fetch_add(&(handle->w_offset), len, __ATOMIC_RELAXED);

    while(l_write + len > handle->size) {
        /**
         * Here l_write may be > size
         * There will never be a situation where while cond is true and below if cond is true
         * in 2 threads
         */
        if(l_write < handle->size)   // update marker, wrap around from here
            handle->marker = l_write;   // no need to do it atomic, if condition will be true only in one thread

        while(1) {
            int w = __atomic_load_n(&(handle->w_offset), __ATOMIC_RELAXED); // here w > size

            if(w < handle->size) // some thread chaged the w_offset, already wraped around
                break;

            if(__atomic_compare_exchange_n(&(handle->w_offset), &w, 0, false, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) {
                // console_log("Wrap around. w_offset: %d", w);
                break;
            }
        }

        // get new l_write
        l_write = __atomic_fetch_add(&(handle->w_offset), len, __ATOMIC_RELAXED);
    }

    memcpy(handle->buffer + l_write, msg, len);    // write to the ring buffer

    int l_used = __atomic_add_fetch(&(handle->used), len, __ATOMIC_RELAXED);
    
    if(l_used > handle->threshold) {
        console_log("Threshold reached: threshold: %d, used: %d, l_write: %d", handle->threshold, l_used, l_write);
        return 1;
    }

    return 0;
}

int
logc_buffer_read_all(struct logc_buffer *handle, char *read_buff)
{
    int zero = 0;
    int r_offset;
    int w_offset;
    int marker;

    if(!__atomic_compare_exchange_n(&(handle->read_lock), &zero, 1, false, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) {
        console_log("Already read");
        return 0;
    }

    r_offset = __atomic_load_n(&(handle->r_offset), __ATOMIC_RELAXED);
    w_offset = __atomic_load_n(&(handle->w_offset), __ATOMIC_RELAXED);
    marker   = __atomic_load_n(&(handle->marker),   __ATOMIC_RELAXED);

    int len = w_offset - r_offset;
    if(len < 0)
        len = (marker - r_offset) + w_offset;

    handle->r_offset = w_offset; // No need atomic op. Only 1 thread can chage this value
    __atomic_sub_fetch(&(handle->used), len, __ATOMIC_RELAXED);

    // No need atomic op. Only 1 thread can chage this value
    handle->read_lock = 0;

    // Now read the message
    if(r_offset == w_offset) {
        return 0;
    }

    int n;
    if(r_offset < w_offset) {
        n = w_offset - r_offset;
        memcpy(read_buff, handle->buffer + r_offset, n);
        read_buff[n] = '\0';
    }
    else {
        int n = marker - r_offset;
        memcpy(read_buff, handle->buffer + r_offset, n);
        memcpy(read_buff + n, handle->buffer, w_offset);
        n += w_offset;
        read_buff[n] = '\0';
    }

    return len;
}
