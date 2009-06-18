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

#ifdef __x86_64__

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
    assert(sizeof(cursor->_mcontext_data.__ss) == sizeof(x86_thread_state64_t));
    assert(sizeof(cursor->_mcontext_data.__es) == sizeof(x86_exception_state64_t));
    assert(sizeof(cursor->_mcontext_data.__fs) == sizeof(x86_float_state64_t));
    
    // thread state
    state_count = x86_THREAD_STATE64_COUNT;
    kr = thread_get_state(thread, x86_THREAD_STATE64, (thread_state_t) &cursor->_mcontext_data.__ss, &state_count);
    if (kr != KERN_SUCCESS) {
        PLCF_DEBUG("Fetch of x86-64 thread state failed with mach error: %d", kr);
        return PLFRAME_INTERNAL;
    }
    
    // floating point state
    state_count = x86_FLOAT_STATE64_COUNT;
    kr = thread_get_state(thread, x86_FLOAT_STATE64, (thread_state_t) &cursor->_mcontext_data.__fs, &state_count);
    if (kr != KERN_SUCCESS) {
        PLCF_DEBUG("Fetch of x86-64 float state failed with mach error: %d", kr);
        return PLFRAME_INTERNAL;
    }
    
    // exception state
    state_count = x86_EXCEPTION_STATE64_COUNT;
    kr = thread_get_state(thread, x86_EXCEPTION_STATE64, (thread_state_t) &cursor->_mcontext_data.__es, &state_count);
    if (kr != KERN_SUCCESS) {
        PLCF_DEBUG("Fetch of x86-64 exception state failed with mach error: %d", kr);
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
        if (cursor->fp[0] == NULL) {
            /* No frame data has been loaded, fetch it from register state */
            kr = plframe_read_addr((void *) cursor->uap->uc_mcontext->__ss.__rbp, cursor->fp, sizeof(cursor->fp));
        } else {
            /* Frame data loaded, walk the stack */
            kr = plframe_read_addr(cursor->fp[0], cursor->fp, sizeof(cursor->fp));
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
    ucontext_t *uap = cursor->uap;
    
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
            RETGEN(rax, ss, uap, reg);

        case PLFRAME_X86_64_RBX:
            RETGEN(rbx, ss, uap, reg);

        case PLFRAME_X86_64_RCX:
            RETGEN(rcx, ss, uap, reg);
            
        case PLFRAME_X86_64_RDX:
            RETGEN(rdx, ss, uap, reg);
            
        case PLFRAME_X86_64_RDI:
            RETGEN(rdi, ss, uap, reg);
            
        case PLFRAME_X86_64_RSI:
            RETGEN(rsi, ss, uap, reg);
            
        case PLFRAME_X86_64_RBP:
            RETGEN(rbp, ss, uap, reg);
            
        case PLFRAME_X86_64_RSP:
            RETGEN(rsp, ss, uap, reg);
            
        case PLFRAME_X86_64_R10:
            RETGEN(r10, ss, uap, reg);
            
        case PLFRAME_X86_64_R11:
            RETGEN(r11, ss, uap, reg);
            
        case PLFRAME_X86_64_R12:
            RETGEN(r12, ss, uap, reg);
            
        case PLFRAME_X86_64_R13:
            RETGEN(r13, ss, uap, reg);
            
        case PLFRAME_X86_64_R14:    
            RETGEN(r14, ss, uap, reg);
            
        case PLFRAME_X86_64_R15:
            RETGEN(r15, ss, uap, reg);
            
        case PLFRAME_X86_64_RIP:
            RETGEN(rip, ss, uap, reg);
            
        case PLFRAME_X86_64_RFLAGS:
            RETGEN(rflags, ss, uap, reg);
            
        case PLFRAME_X86_64_CS:
            RETGEN(cs, ss, uap, reg);
            
        case PLFRAME_X86_64_FS:
            RETGEN(fs, ss, uap, reg);
            
        case PLFRAME_X86_64_GS:
            RETGEN(gs, ss, uap, reg);
            
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
