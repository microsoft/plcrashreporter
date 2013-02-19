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
#import <assert.h>
#import <stdlib.h>

#define RETGEN(name, type, ts, result) {\
    *result = (ts->x86_state. type . __ ## name); \
    return PLFRAME_ESUCCESS; \
}

#ifdef __x86_64__

// PLFrameWalker API
plframe_error_t plframe_cursor_next (plframe_cursor_t *cursor) {
    kern_return_t kr;
    void *prevfp = cursor->fp[0];
    
    /* Fetch the next stack address */
    if (cursor->init_frame) {
        /* The first frame is already available, so there's nothing to do */
        cursor->init_frame = false;
        return PLFRAME_ESUCCESS;
    } else {
        if (cursor->fp[0] == NULL) {
            /* No frame data has been loaded, fetch it from register state */
            kr = plcrash_async_read_addr(mach_task_self(), cursor->thread_state.x86_state.thread.uts.ts64.__rbp, cursor->fp, sizeof(cursor->fp));
        } else {
            /* Frame data loaded, walk the stack */
            kr = plcrash_async_read_addr(mach_task_self(), (pl_vm_address_t) cursor->fp[0], cursor->fp, sizeof(cursor->fp));
        }
    }
    
    /* Was the read successful? */
    if (kr != KERN_SUCCESS)
        return PLFRAME_EBADFRAME;
    
    /* Check for completion */
    if (cursor->fp[0] == NULL)
        return PLFRAME_ENOFRAME;
    
    /* Is the stack growing in the right direction? */
    if (!cursor->init_frame && prevfp > cursor->fp[0])
        return PLFRAME_EBADFRAME;
    
    /* New frame fetched */
    return PLFRAME_ESUCCESS;
}


// PLFrameWalker API
plframe_error_t plframe_get_reg (plframe_cursor_t *cursor, plframe_regnum_t regnum, plframe_greg_t *reg) {
    plframe_thread_state_t *ts = &cursor->thread_state;
    
    /* Supported register for this context state? */
    if (cursor->fp[0] != NULL) {
        if (regnum == PLFRAME_X86_64_RIP) {
            *reg = (plframe_greg_t) cursor->fp[1];
            return PLFRAME_ESUCCESS;
        }
        
        return PLFRAME_ENOTSUP;
    }

    switch (regnum) {
        case PLFRAME_X86_64_RAX:
            RETGEN(rax, thread.uts.ts64, ts, reg);

        case PLFRAME_X86_64_RBX:
            RETGEN(rbx, thread.uts.ts64, ts, reg);

        case PLFRAME_X86_64_RCX:
            RETGEN(rcx, thread.uts.ts64, ts, reg);
            
        case PLFRAME_X86_64_RDX:
            RETGEN(rdx, thread.uts.ts64, ts, reg);
            
        case PLFRAME_X86_64_RDI:
            RETGEN(rdi, thread.uts.ts64, ts, reg);
            
        case PLFRAME_X86_64_RSI:
            RETGEN(rsi, thread.uts.ts64, ts, reg);
            
        case PLFRAME_X86_64_RBP:
            RETGEN(rbp, thread.uts.ts64, ts, reg);
            
        case PLFRAME_X86_64_RSP:
            RETGEN(rsp, thread.uts.ts64, ts, reg);
            
        case PLFRAME_X86_64_R10:
            RETGEN(r10, thread.uts.ts64, ts, reg);
            
        case PLFRAME_X86_64_R11:
            RETGEN(r11, thread.uts.ts64, ts, reg);
            
        case PLFRAME_X86_64_R12:
            RETGEN(r12, thread.uts.ts64, ts, reg);
            
        case PLFRAME_X86_64_R13:
            RETGEN(r13, thread.uts.ts64, ts, reg);
            
        case PLFRAME_X86_64_R14:    
            RETGEN(r14, thread.uts.ts64, ts, reg);
            
        case PLFRAME_X86_64_R15:
            RETGEN(r15, thread.uts.ts64, ts, reg);
            
        case PLFRAME_X86_64_RIP:
            RETGEN(rip, thread.uts.ts64, ts, reg);
            
        case PLFRAME_X86_64_RFLAGS:
            RETGEN(rflags, thread.uts.ts64, ts, reg);
            
        case PLFRAME_X86_64_CS:
            RETGEN(cs, thread.uts.ts64, ts, reg);
            
        case PLFRAME_X86_64_FS:
            RETGEN(fs, thread.uts.ts64, ts, reg);
            
        case PLFRAME_X86_64_GS:
            RETGEN(gs, thread.uts.ts64, ts, reg);
            
        default:
            // Unsupported register
            break;
    }
    
    return PLFRAME_ENOTSUP;
}

// PLFrameWalker API
plframe_error_t plframe_get_freg (plframe_cursor_t *cursor, plframe_regnum_t regnum, plframe_fpreg_t *fpreg) {
    return PLFRAME_ENOTSUP;
}

// PLFrameWalker API
const char *plframe_get_regname (plframe_regnum_t regnum) {
    switch (regnum) {
        case PLFRAME_X86_64_RAX:
            return "rax";

        case PLFRAME_X86_64_RBX:
            return "rbx";
            
        case PLFRAME_X86_64_RCX:
            return "rcx";
            
        case PLFRAME_X86_64_RDX:
            return "rdx";
            
        case PLFRAME_X86_64_RDI:
            return "rdi";
            
        case PLFRAME_X86_64_RSI:
            return "rsi";
            
        case PLFRAME_X86_64_RBP:
            return "rbp";
            
        case PLFRAME_X86_64_RSP:
            return "rsp";
            
        case PLFRAME_X86_64_R10:
            return "r10";
            
        case PLFRAME_X86_64_R11:
            return "r11";
            
        case PLFRAME_X86_64_R12:
            return "r12";
            
        case PLFRAME_X86_64_R13:
            return "r13";
            
        case PLFRAME_X86_64_R14:    
            return "r14";
            
        case PLFRAME_X86_64_R15:
            return "r15";
            
        case PLFRAME_X86_64_RIP:
            return "rip";
            
        case PLFRAME_X86_64_RFLAGS:
            return "rflags";
            
        case PLFRAME_X86_64_CS:
            return "cs";
            
        case PLFRAME_X86_64_FS:
            return "fs";
            
        case PLFRAME_X86_64_GS:
            return "gs";
            
        default:
            // Unsupported register
            break;
    }
    
    /* Unsupported register is an implementation error (checked in unit tests) */
    PLCF_DEBUG("Missing register name for register id: %d", regnum);
    abort();
}

#endif
