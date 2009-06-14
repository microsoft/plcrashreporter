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

#ifdef __ppc__

// PLFrameWalker API
plframe_error_t plframe_cursor_init (plframe_cursor_t *cursor, ucontext_t *uap) {
    return PLFRAME_ENOTSUP;
}

// PLFrameWalker API
plframe_error_t plframe_cursor_thread_init (plframe_cursor_t *cursor, thread_t thread) {
    return PLFRAME_ENOTSUP;
}


// PLFrameWalker API
plframe_error_t plframe_cursor_next (plframe_cursor_t *cursor) {
    return PLFRAME_ENOTSUP;
}


// PLFrameWalker API
plframe_error_t plframe_get_reg (plframe_cursor_t *cursor, plframe_regnum_t regnum, plframe_greg_t *reg) {
    return PLFRAME_ENOTSUP;
}

// PLFrameWalker API
plframe_error_t plframe_get_freg (plframe_cursor_t *cursor, plframe_regnum_t regnum, plframe_fpreg_t *fpreg) {
    return PLFRAME_ENOTSUP;
}

// PLFrameWalker API
const char *plframe_get_regname (plframe_regnum_t regnum) {
    switch (regnum) {
        case PLFRAME_PPC_SRR0:
            return "srr0";
        case PLFRAME_PPC_SRR1:
            return "srr1";
            
        case PLFRAME_PPC_DAR:
            return "dar";
        case PLFRAME_PPC_DSISR:
            return "dsisr";

        case PLFRAME_PPC_R0:
            return "r0";
        case PLFRAME_PPC_R1:
            return "r1";
        case PLFRAME_PPC_R2:
            return "r2";
        case PLFRAME_PPC_R3:
            return "r3";
        case PLFRAME_PPC_R4:
            return "r4";
        case PLFRAME_PPC_R5:
            return "r5";
        case PLFRAME_PPC_R6:
            return "r6";
        case PLFRAME_PPC_R7:
            return "r7";
        case PLFRAME_PPC_R8:
            return "r8";
        case PLFRAME_PPC_R9:
            return "r9";
        case PLFRAME_PPC_R10:
            return "r10";
        case PLFRAME_PPC_R11:
            return "r11";
        case PLFRAME_PPC_R12:
            return "r12";
        case PLFRAME_PPC_R13:
            return "r13";
        case PLFRAME_PPC_R14:
            return "r14";
        case PLFRAME_PPC_R15:
            return "r15";
        case PLFRAME_PPC_R16:
            return "r16";
        case PLFRAME_PPC_R17:
            return "r17";
        case PLFRAME_PPC_R18:
            return "r18";
        case PLFRAME_PPC_R19:
            return "r19";
        case PLFRAME_PPC_R20:
            return "r20";
        case PLFRAME_PPC_R21:
            return "r21";
        case PLFRAME_PPC_R22:
            return "r22";
        case PLFRAME_PPC_R23:
            return "r23";
        case PLFRAME_PPC_R24:
            return "r24";
        case PLFRAME_PPC_R25:
            return "r25";
        case PLFRAME_PPC_R26:
            return "r26";
        case PLFRAME_PPC_R27:
            return "r27";
        case PLFRAME_PPC_R28:
            return "r28";
        case PLFRAME_PPC_R29:
            return "r29";
        case PLFRAME_PPC_R30:
            return "r30";
        case PLFRAME_PPC_R31:
            return "r31";
            
        case PLFRAME_PPC_CR:
            return "cr";
            
        case PLFRAME_PPC_XER:
            return "xer";
            
        case PLFRAME_PPC_LR:
            return "lr";
            
        case PLFRAME_PPC_CTR:
            return "ctr";
            
        case PLFRAME_PPC_VRSAVE:
            return "vrsave";

        default:
            // Unsupported register
            break;
    }
    
    /* Unsupported register is an implementation error (checked in unit tests) */
    PLCF_DEBUG("Missing register name for register id: %d", regnum);
    abort();
}

#endif
