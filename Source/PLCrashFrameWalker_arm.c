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
plframe_error_t plframe_cursor_get_reg (plframe_cursor_t *cursor, plframe_regnum_t regnum, plframe_greg_t *reg) {
    plframe_thread_state_t *ts = &cursor->thread_state;

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
            RETGEN(r[0], thread, ts, reg);

        case PLFRAME_ARM_R1:
            RETGEN(r[1], thread, ts, reg);

        case PLFRAME_ARM_R2:
            RETGEN(r[2], thread, ts, reg);

        case PLFRAME_ARM_R3:
            RETGEN(r[3], thread, ts, reg);

        case PLFRAME_ARM_R4:
            RETGEN(r[4], thread, ts, reg);

        case PLFRAME_ARM_R5:
            RETGEN(r[5], thread, ts, reg);

        case PLFRAME_ARM_R6:
            RETGEN(r[6], thread, ts, reg);

        case PLFRAME_ARM_R7:
            RETGEN(r[7], thread, ts, reg);

        case PLFRAME_ARM_R8:
            RETGEN(r[8], thread, ts, reg);

        case PLFRAME_ARM_R9:
            RETGEN(r[9], thread, ts, reg);

        case PLFRAME_ARM_R10:
            RETGEN(r[10], thread, ts, reg);

        case PLFRAME_ARM_R11:
            RETGEN(r[11], thread, ts, reg);

        case PLFRAME_ARM_R12:
            RETGEN(r[12], thread, ts, reg);

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
size_t plframe_cursor_get_regcount (plframe_cursor_t *cursor) {
    /* Last is an index value, so increment to get the count */
    return PLFRAME_ARM_LAST_REG+1;
}

// PLFrameWalker API
plframe_error_t plframe_cursor_get_freg (plframe_cursor_t *cursor, plframe_regnum_t regnum, plframe_fpreg_t *fpreg) {
    return PLFRAME_ENOTSUP;
}


// PLFrameWalker API
const char *plframe_cursor_get_regname (plframe_cursor_t *cursor, plframe_regnum_t regnum) {
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
