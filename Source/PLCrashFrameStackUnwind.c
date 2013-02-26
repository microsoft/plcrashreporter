/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008-2013 Plausible Labs Cooperative, Inc.
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "PLCrashFrameStackUnwind.h"
#include "PLCrashAsync.h"

/**
 * Fetch the next frame.
 *
 * @param cursor A cursor instance initialized with plframe_cursor_init();
 * @return Returns PLFRAME_ESUCCESS on success, PLFRAME_ENOFRAME is no additional frames are available, or a standard plframe_error_t code if an error occurs.
 *
 * @todo The stack direction and interpretation of the frame data should be moved to the central platform configuration.
 */
plframe_error_t plframe_cursor_next_fp (plframe_cursor_t *cursor) {
    kern_return_t kr;
    
    if (cursor->depth == 0) {
        /* The first frame is already available via the thread state. */
        cursor->frame.fp = plcrash_async_thread_state_get_reg(&cursor->thread_state, PLCRASH_REG_FP);
        cursor->frame.pc = plcrash_async_thread_state_get_reg(&cursor->thread_state, PLCRASH_REG_IP);
        cursor->depth++;

        return PLFRAME_ESUCCESS;
    } else if (cursor->depth > 1) {
#if defined(__arm__) || defined(__i386__) || defined(__x86_64__)
        /* Is the stack growing in the right direction? */
        if (cursor->frame.fp < cursor->prev_frame.fp) {
            PLCF_DEBUG("Stack growing in wrong direction, terminating stack walk");
            return PLFRAME_EBADFRAME;
        }
#else
#error Define the direction this stack grows
#endif
    }

    /* Read in the next frame. */
    void *fdata[PLFRAME_STACKFRAME_LEN]; // TODO - this is not 32-bit/64-bit safe
    kr = plcrash_async_read_addr(cursor->task, (pl_vm_address_t) cursor->frame.fp, fdata, sizeof(fdata));

    /* Was the read successful? */
    if (kr != KERN_SUCCESS) {
        PLCF_DEBUG("Failed to read frame: %d", kr);
        return PLFRAME_EBADFRAME;
    }

    /* Extract the frame data */
    plframe_stackframe_t frame;
#if defined(__arm__) || defined(__i386__) || defined(__x86_64__)
    frame.fp = fdata[0];
    frame.pc = fdata[1];
#else
#error Add platform support
#endif

    /* Check for completion */
    if (frame.fp == 0x0)
        return PLFRAME_ENOFRAME;

    /* New frame fetched */
    cursor->prev_frame = cursor->frame;
    cursor->frame = frame;
    cursor->depth++;

    return PLFRAME_ESUCCESS;
}