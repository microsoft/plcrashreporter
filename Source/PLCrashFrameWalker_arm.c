/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008-2009 Plausible Labs Cooperative, Inc.
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

#import "PLCrashFrameWalker.h"
#import "PLCrashAsync.h"

#import <signal.h>
#import <stdlib.h>
#import <assert.h>

#ifdef __arm__

// PLFrameWalker API
plframe_error_t plframe_cursor_read_stackframe (plframe_cursor_t *cursor, plframe_stackframe_t *frame) {
    uint32_t fdata[2];
    kern_return_t kr;

    /* Read the frame */
    kr = plcrash_async_read_addr(cursor->task, (pl_vm_address_t) cursor->frame.fp, fdata, sizeof(fdata));
    if (kr != KERN_SUCCESS) {
        PLCF_DEBUG("Failed to read frame: %d", kr);
        return PLFRAME_EBADFRAME;
    }
    
    /* Extract the data */
    frame->fp = fdata[0];
    frame->pc = fdata[1];
    
    return PLFRAME_ESUCCESS;
}

// PLFrameWalker API
plframe_error_t plframe_cursor_get_reg (plframe_cursor_t *cursor, plcrash_regnum_t regnum, plcrash_greg_t *reg) {
    /* Fetch from thread state */
    if (cursor->frame.depth == 1) {
        *reg = plcrash_async_thread_state_get_reg(&cursor->thread_state, regnum);
        return PLFRAME_ESUCCESS;
    }

    /* Fetch from the stack walker */
    if (regnum == PLCRASH_ARM_PC) {
        *reg = cursor->frame.pc;
        return PLFRAME_ESUCCESS;
    }
    
    return PLFRAME_ENOTSUP;
}

// PLFrameWalker API
size_t plframe_cursor_get_regcount (plframe_cursor_t *cursor) {
    return plcrash_async_thread_state_get_reg_count(&cursor->thread_state);
}

// PLFrameWalker API
const char *plframe_cursor_get_regname (plframe_cursor_t *cursor, plcrash_regnum_t regnum) {
    return plcrash_async_thread_state_get_reg_name(&cursor->thread_state, regnum);
}


#endif /* __arm__ */
