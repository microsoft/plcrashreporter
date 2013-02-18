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

#define RETGEN(name, type, ts, result) {\
    *result = (ts->arm_state. type . __ ## name); \
    return PLFRAME_ESUCCESS; \
}

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
            kr = plcrash_async_read_addr(mach_task_self(), cursor->thread_state.arm_state.thread.__r[7], cursor->fp, sizeof(cursor->fp));
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
    plframe_cursor_thread_state_t *ts = &cursor->thread_state;

    /* Supported register for this context state? */
    if (cursor->fp[0] != NULL) {
        if (regnum == PLFRAME_ARM_PC) {
            *reg = (plframe_greg_t) cursor->fp[1];
            return PLFRAME_ESUCCESS;
        }
        
        return PLFRAME_ENOTSUP;
    }
    
    switch (regnum) {
        case PLFRAME_ARM_R0:
        case PLFRAME_ARM_R1:
        case PLFRAME_ARM_R2:
        case PLFRAME_ARM_R3:
        case PLFRAME_ARM_R4:
        case PLFRAME_ARM_R5:
        case PLFRAME_ARM_R6:
        case PLFRAME_ARM_R7:
        case PLFRAME_ARM_R8:
        case PLFRAME_ARM_R9:
        case PLFRAME_ARM_R10:
        case PLFRAME_ARM_R11:
        case PLFRAME_ARM_R12:
            // Map enum to actual register index */
            RETGEN(r[regnum - PLFRAME_ARM_R0], thread, ts, reg);

        case PLFRAME_ARM_SP:
            RETGEN(sp, thread, ts, reg);

        case PLFRAME_ARM_LR:
            RETGEN(lr, thread, ts, reg);

        case PLFRAME_ARM_PC:
            RETGEN(pc, thread, ts, reg);
            
        case PLFRAME_ARM_CPSR:
            RETGEN(cpsr, thread, ts, reg);
            
        default:
            return PLFRAME_ENOTSUP;
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
        case PLFRAME_ARM_R0:
            return "r0";
        case PLFRAME_ARM_R1:
            return "r1";
        case PLFRAME_ARM_R2:
            return "r2";
        case PLFRAME_ARM_R3:
            return "r3";
        case PLFRAME_ARM_R4:
            return "r4";
        case PLFRAME_ARM_R5:
            return "r5";
        case PLFRAME_ARM_R6:
            return "r6";
        case PLFRAME_ARM_R7:
            return "r7";
        case PLFRAME_ARM_R8:
            return "r8";
        case PLFRAME_ARM_R9:
            return "r9";
        case PLFRAME_ARM_R10:
            return "r10";
        case PLFRAME_ARM_R11:
            return "r11";
        case PLFRAME_ARM_R12:
            return "r12";
            
        case PLFRAME_ARM_SP:
            return "sp";
            
        case PLFRAME_ARM_LR:
            return "lr";
            
        case PLFRAME_ARM_PC:
            return "pc";

        case PLFRAME_ARM_CPSR:
            return "cpsr";
            
        default:
            // Unsupported register
            break;
    }
    
    /* Unsupported register is an implementation error (checked in unit tests) */
    PLCF_DEBUG("Missing register name for register id: %d", regnum);
    abort();
}


#endif /* __arm__ */
