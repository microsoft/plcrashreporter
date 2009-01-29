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

#ifdef __i386__

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
    assert(sizeof(cursor->_mcontext_data.__ss) == sizeof(x86_thread_state32_t));
    assert(sizeof(cursor->_mcontext_data.__es) == sizeof(x86_exception_state32_t));
    assert(sizeof(cursor->_mcontext_data.__fs) == sizeof(x86_float_state32_t));
    
    // thread state
    state_count = x86_THREAD_STATE32_COUNT;
    kr = thread_get_state(thread, x86_THREAD_STATE32, (thread_state_t) &cursor->_mcontext_data.__ss, &state_count);
    if (kr != KERN_SUCCESS) {
        PLCF_DEBUG("Fetch of x86 thread state failed with mach error: %d", kr);
        return PLFRAME_INTERNAL;
    }

    // floating point state
    state_count = x86_FLOAT_STATE32_COUNT;
    kr = thread_get_state(thread, x86_FLOAT_STATE32, (thread_state_t) &cursor->_mcontext_data.__fs, &state_count);
    if (kr != KERN_SUCCESS) {
        PLCF_DEBUG("Fetch of x86 float state failed with mach error: %d", kr);
        return PLFRAME_INTERNAL;
    }

    // exception state
    state_count = x86_EXCEPTION_STATE32_COUNT;
    kr = thread_get_state(thread, x86_EXCEPTION_STATE32, (thread_state_t) &cursor->_mcontext_data.__es, &state_count);
    if (kr != KERN_SUCCESS) {
        PLCF_DEBUG("Fetch of x86 exception state failed with mach error: %d", kr);
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
            kr = plframe_read_addr((void *) cursor->uap->uc_mcontext->__ss.__ebp, cursor->fp, sizeof(cursor->fp));
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
        if (regnum == PLFRAME_X86_EIP) {
            *reg = (plframe_greg_t) cursor->fp[1];
            return PLFRAME_ESUCCESS;
        }
        
        return PLFRAME_ENOTSUP;
    }

    /* All word-sized registers */
    switch (regnum) {
        case PLFRAME_X86_EAX:
            RETGEN(eax, ss, uap, reg);
    
        case PLFRAME_X86_EDX:
            RETGEN(edx, ss, uap, reg);
            
        case PLFRAME_X86_ECX:
            RETGEN(ecx, ss, uap, reg);

        case PLFRAME_X86_EBX:
            RETGEN(ebx, ss, uap, reg);

        case PLFRAME_X86_EBP:
            RETGEN(ebp, ss, uap, reg);

        case PLFRAME_X86_ESI:
            RETGEN(esi, ss, uap, reg);

        case PLFRAME_X86_EDI:
            RETGEN(edi, ss, uap, reg);

        case PLFRAME_X86_ESP:
            RETGEN(esp, ss, uap, reg);

        case PLFRAME_X86_EIP:
            RETGEN(eip, ss, uap, reg);

        case PLFRAME_X86_EFLAGS:
            RETGEN(eflags, ss, uap, reg);

        case PLFRAME_X86_TRAPNO:
            RETGEN(trapno, es, uap, reg);

        case PLFRAME_X86_CS:
            RETGEN(cs, ss, uap, reg);

        case PLFRAME_X86_DS:
            RETGEN(ds, ss, uap, reg);

        case PLFRAME_X86_ES:
            RETGEN(es, ss, uap, reg);

        case PLFRAME_X86_FS:
            RETGEN(fs, ss, uap, reg);

        case PLFRAME_X86_GS:
            RETGEN(gs, ss, uap, reg);

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
    /* All word-sized registers */
    switch (regnum) {
        case PLFRAME_X86_EAX:
            return "eax";
            
        case PLFRAME_X86_EDX:
            return "edx";
            
        case PLFRAME_X86_ECX:
            return "ecx";
            
        case PLFRAME_X86_EBX:
            return "ebx";
            
        case PLFRAME_X86_EBP:
            return "ebp";
            
        case PLFRAME_X86_ESI:
            return "esi";
            
        case PLFRAME_X86_EDI:
            return "edi";
            
        case PLFRAME_X86_ESP:
            return "esp";
            
        case PLFRAME_X86_EIP:
            return "eip";
            
        case PLFRAME_X86_EFLAGS:
            return "eflags";
            
        case PLFRAME_X86_TRAPNO:
            return "trapno";
            
        case PLFRAME_X86_CS:
            return "cs";
            
        case PLFRAME_X86_DS:
            return "ds";
            
        case PLFRAME_X86_ES:
            return "es";
            
        case PLFRAME_X86_FS:
            return "fs";
            
        case PLFRAME_X86_GS:
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
