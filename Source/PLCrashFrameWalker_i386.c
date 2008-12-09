/*
 *  PLCrashFrameWalker_i386.c
 *  CrashReporter
 *
 *  Created by Landon Fuller on 12/8/08.
 *  Copyright 2008 Plausible Labs Cooperative, Inc.. All rights reserved.
 *
 */

#include "PLCrashFrameWalker.h"
#include <signal.h>
#include <assert.h>

#define RETGEN(name, type, uap, result) {\
    *result = (uap->uc_mcontext->__ ## type . __ ## name); \
    return PLFRAME_ESUCCESS; \
}


#ifdef __i386__

// PLFrameWalker API
plframe_error_t plframe_cursor_init (plframe_cursor_t *cursor, ucontext_t *uap) {
    cursor->uap = uap;
    cursor->init_frame = true;
    cursor->fp = NULL;

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
        PLCF_DEBUG("Fetch of x86 float state failed with mach error: %d", kr);
        return PLFRAME_INTERNAL;
    }

    // stack

    /* Perform standard initialization */
    plframe_cursor_init(cursor, uap);

    return PLFRAME_ESUCCESS;
}


// PLFrameWalker API
plframe_error_t plframe_cursor_next (plframe_cursor_t *cursor) {

    /* Fetch the next stack address */
    if (cursor->fp == NULL) {
        cursor->fp = (void **) cursor->uap->uc_mcontext->__ss.__ebp;
    } else {
        cursor->init_frame = false;
        cursor->fp = cursor->fp[0];
    }
    
    /* Check for completion */
    if (cursor->fp == NULL)
        return PLFRAME_ENOFRAME;

    /* Check for a valid address */
    if (!plframe_valid_stackaddr(cursor->uap, cursor->fp)) {
        return PLFRAME_EBADFRAME;
    }

    /* New frame fetched */
    return PLFRAME_ESUCCESS;
}


// PLFrameWalker API
plframe_error_t plframe_get_reg (plframe_cursor_t *cursor, plframe_regnum_t regnum, plframe_word_t *reg) {
    ucontext_t *uap = cursor->uap;
    
    /* Supported register for this context state? */
    if (!cursor->init_frame) {
        if (regnum == PLFRAME_X86_EIP) {
            *reg = (plframe_word_t) cursor->fp[1];
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

#endif
