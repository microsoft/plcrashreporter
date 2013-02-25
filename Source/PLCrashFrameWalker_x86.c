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

#if defined(__i386__) || defined(__x86_64__)

// PLFrameWalker API
plframe_error_t plframe_cursor_get_reg (plframe_cursor_t *cursor, plcrash_regnum_t regnum, plcrash_greg_t *reg) {
    /* Fetch from the stack walker */
    if (cursor->fp[0] != NULL) {
        bool x64 = cursor->thread_state.x86_state.thread.tsh.flavor == x86_THREAD_STATE64;
        
        if ((x64 && regnum == PLCRASH_X86_64_RIP) || (!x64 && regnum == PLCRASH_X86_EIP)) {
            *reg = (plcrash_greg_t) cursor->fp[1];
            return PLFRAME_ESUCCESS;
        }
        
        return PLFRAME_ENOTSUP;
    }

    /* Fetch from thread state */
    *reg = plcrash_async_thread_state_get_reg(&cursor->thread_state, regnum);
    return PLFRAME_ESUCCESS;
}

// PLFrameWalker API
char const *plframe_cursor_get_regname (plframe_cursor_t *cursor, plcrash_regnum_t regnum) {
    return plcrash_async_thread_state_get_reg_name(&cursor->thread_state, regnum);
}

// PLFrameWalker API
size_t plframe_cursor_get_regcount (plframe_cursor_t *cursor) {
    return plcrash_async_thread_state_get_reg_count(&cursor->thread_state);
}

#endif /* defined(__i386__) || defined(__x86_64__) */
