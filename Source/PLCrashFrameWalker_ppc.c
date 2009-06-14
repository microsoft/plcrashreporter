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

#define RETGEN(name, type, uap, result) {\
    *result = (uap->uc_mcontext->__ ## type . __ ## name); \
    return PLFRAME_ESUCCESS; \
}

#ifdef __ppc__

// PLFrameWalker API
plframe_error_t plframe_cursor_init (plframe_cursor_t *cursor, ucontext_t *uap) {
    cursor->uap = uap;
    cursor->init_frame = true;
    cursor->fp[0] = NULL;

    return PLFRAME_ESUCCESS;
}

// PLFrameWalker API
plframe_error_t plframe_cursor_thread_init (plframe_cursor_t *cursor, thread_t thread) {
    kern_return_t kr;
    ucontext_t *uap;
    
    /* Perform basic initialization */
    uap = &cursor->_uap_data;
    uap->uc_mcontext = (void *) &cursor->_mcontext_data;
    
    /* Zero the signal mask */
    sigemptyset(&uap->uc_sigmask);
    
    /* Fetch the thread states */
    mach_msg_type_number_t state_count;
    
    /* Sanity check */
    assert(sizeof(cursor->_mcontext_data.__ss) == sizeof(ppc_thread_state_t));
    assert(sizeof(cursor->_mcontext_data.__es) == sizeof(ppc_exception_state_t));
    
    // thread state
    state_count = PPC_THREAD_STATE_COUNT;
    kr = thread_get_state(thread, PPC_THREAD_STATE, (thread_state_t) &cursor->_mcontext_data.__ss, &state_count);
    if (kr != KERN_SUCCESS) {
        PLCF_DEBUG("Fetch of PPC thread state failed with mach error: %d", kr);
        return PLFRAME_INTERNAL;
    }
    
    // exception state
    state_count = PPC_EXCEPTION_STATE_COUNT;
    kr = thread_get_state(thread, PPC_EXCEPTION_STATE, (thread_state_t) &cursor->_mcontext_data.__es, &state_count);
    if (kr != KERN_SUCCESS) {
        PLCF_DEBUG("Fetch of PPC exception state failed with mach error: %d", kr);
        return PLFRAME_INTERNAL;
    }
    
    /* Perform standard initialization */
    plframe_cursor_init(cursor, uap);
    
    return PLFRAME_ESUCCESS;
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
        kr = KERN_SUCCESS;

        if (cursor->fp[0] == NULL) {
            /* No frame data has been loaded, fetch it from register state */
            kr = plframe_read_addr((void *) cursor->uap->uc_mcontext->__ss.__r1, cursor->fp, sizeof(cursor->fp));
        }
        
        if (kr == KERN_SUCCESS) {
            /* Frame data loaded, walk the stack */
            kr = plframe_read_addr(cursor->fp[0], cursor->fp, sizeof(cursor->fp));
        }
    }
    
    /* Was the read successful? */
    if (kr != KERN_SUCCESS) {
        return PLFRAME_EBADFRAME;
    }

    /* Check for completion */
    if (cursor->fp[0] == NULL) {
        return PLFRAME_ENOFRAME;
    }
    
    /* Is the stack growing in the right direction? */
    if (!cursor->init_frame && prevfp > cursor->fp[0])
        return PLFRAME_EBADFRAME;
    
    /* New frame fetched */
    return PLFRAME_ESUCCESS;
}


// PLFrameWalker API
plframe_error_t plframe_get_reg (plframe_cursor_t *cursor, plframe_regnum_t regnum, plframe_greg_t *reg) {
    ucontext_t *uap = cursor->uap;
    
    /* Supported register for this context state? */
    if (cursor->fp[0] != NULL) {
        if (regnum == PLFRAME_PDEF_REG_IP) {
            *reg = (plframe_greg_t) cursor->fp[2] - 4; // offset by 1 instruction
            return PLFRAME_ESUCCESS;
        }
        
        return PLFRAME_ENOTSUP;
    }
    
    /* All GP registers */
    switch (regnum) {
        case PLFRAME_PPC_SRR0:
            RETGEN(srr0, ss, uap, reg);
            
        case PLFRAME_PPC_SRR1:
            RETGEN(srr1, ss, uap, reg);
            
        case PLFRAME_PPC_DAR:
            RETGEN(dar, es, uap, reg);

        case PLFRAME_PPC_DSISR:
            RETGEN(dsisr, es, uap, reg);

            
        case PLFRAME_PPC_R0:
            RETGEN(r0, ss, uap, reg);
        case PLFRAME_PPC_R1:
            RETGEN(r1, ss, uap, reg);
        case PLFRAME_PPC_R2:
            RETGEN(r2, ss, uap, reg);
        case PLFRAME_PPC_R3:
            RETGEN(r3, ss, uap, reg);
        case PLFRAME_PPC_R4:
            RETGEN(r4, ss, uap, reg);
        case PLFRAME_PPC_R5:
            RETGEN(r5, ss, uap, reg);
        case PLFRAME_PPC_R6:
            RETGEN(r6, ss, uap, reg);
        case PLFRAME_PPC_R7:
            RETGEN(r7, ss, uap, reg);
        case PLFRAME_PPC_R8:
            RETGEN(r8, ss, uap, reg);
        case PLFRAME_PPC_R9:
            RETGEN(r9, ss, uap, reg);
        case PLFRAME_PPC_R10:
            RETGEN(r10, ss, uap, reg);
        case PLFRAME_PPC_R11:
            RETGEN(r11, ss, uap, reg);
        case PLFRAME_PPC_R12:
            RETGEN(r12, ss, uap, reg);
        case PLFRAME_PPC_R13:
            RETGEN(r13, ss, uap, reg);
        case PLFRAME_PPC_R14:
            RETGEN(r14, ss, uap, reg);
        case PLFRAME_PPC_R15:
            RETGEN(r15, ss, uap, reg);
        case PLFRAME_PPC_R16:
            RETGEN(r16, ss, uap, reg);
        case PLFRAME_PPC_R17:
            RETGEN(r17, ss, uap, reg);
        case PLFRAME_PPC_R18:
            RETGEN(r18, ss, uap, reg);
        case PLFRAME_PPC_R19:
            RETGEN(r19, ss, uap, reg);
        case PLFRAME_PPC_R20:
            RETGEN(r20, ss, uap, reg);
        case PLFRAME_PPC_R21:
            RETGEN(r21, ss, uap, reg);
        case PLFRAME_PPC_R22:
            RETGEN(r22, ss, uap, reg);
        case PLFRAME_PPC_R23:
            RETGEN(r23, ss, uap, reg);
        case PLFRAME_PPC_R24:
            RETGEN(r24, ss, uap, reg);
        case PLFRAME_PPC_R25:
            RETGEN(r25, ss, uap, reg);
        case PLFRAME_PPC_R26:
            RETGEN(r26, ss, uap, reg);
        case PLFRAME_PPC_R27:
            RETGEN(r27, ss, uap, reg);
        case PLFRAME_PPC_R28:
            RETGEN(r28, ss, uap, reg);
        case PLFRAME_PPC_R29:
            RETGEN(r29, ss, uap, reg);
        case PLFRAME_PPC_R30:
            RETGEN(r30, ss, uap, reg);
        case PLFRAME_PPC_R31:
            RETGEN(r31, ss, uap, reg);
            
        case PLFRAME_PPC_CR:
            RETGEN(cr, ss, uap, reg);
            
        case PLFRAME_PPC_XER:
            RETGEN(xer, ss, uap, reg);
            
        case PLFRAME_PPC_LR:
            RETGEN(lr, ss, uap, reg);
            
        case PLFRAME_PPC_CTR:
            RETGEN(ctr, ss, uap, reg);
            
        case PLFRAME_PPC_VRSAVE:
            RETGEN(vrsave, ss, uap, reg);

        default:
            // Unsupported register
            return PLFRAME_EBADREG;
    }
    
    /* Shouldn't be reachable */
    return PLFRAME_EUNKNOWN;
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
