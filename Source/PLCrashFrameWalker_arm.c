/*
 *  PLCrashFrameWalker_arm.c
 *  CrashReporter
 *
 *  Created by Landon Fuller on 12/8/08.
 *  Copyright 2008 Plausible Labs Cooperative, Inc.. All rights reserved.
 *
 */

#include "PLCrashFrameWalker.h"

#include <signal.h>
#include <assert.h>

#ifdef __arm__

// Minimum readable size of a stack frame
#define MIN_VALID_STACK (sizeof(intptr_t) * 2)

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
    assert(sizeof(cursor->_mcontext_data.__ss) == sizeof(arm_thread_state_t));
    assert(sizeof(cursor->_mcontext_data.__es) == sizeof(arm_exception_state_t));
    assert(sizeof(cursor->_mcontext_data.__fs) == sizeof(arm_vfp_state_t));
    
    // thread state
    state_count = ARM_THREAD_STATE_COUNT;
    kr = thread_get_state(thread, ARM_THREAD_STATE, (thread_state_t) &cursor->_mcontext_data.__ss, &state_count);
    if (kr != KERN_SUCCESS) {
        PLCF_DEBUG("Fetch of arm thread state failed with mach error: %d", kr);
        return PLFRAME_INTERNAL;
    }
    
    // floating point state
    state_count = ARM_VFP_STATE_COUNT;
    kr = thread_get_state(thread, ARM_VFP_STATE, (thread_state_t) &cursor->_mcontext_data.__fs, &state_count);
    if (kr != KERN_SUCCESS) {
        PLCF_DEBUG("Fetch of arm vfp state failed with mach error: %d", kr);
        return PLFRAME_INTERNAL;
    }
    
    // exception state
    state_count = ARM_EXCEPTION_STATE_COUNT;
    kr = thread_get_state(thread, ARM_EXCEPTION_STATE, (thread_state_t) &cursor->_mcontext_data.__es, &state_count);
    if (kr != KERN_SUCCESS) {
        PLCF_DEBUG("Fetch of ARM exception state failed with mach error: %d", kr);
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
    if (cursor->fp[0] == NULL) {
        kr = plframe_read_addr((void *) cursor->uap->uc_mcontext->__ss.__r[7], cursor->fp, sizeof(cursor->fp));
    } else {
        cursor->init_frame = false;
        kr = plframe_read_addr(cursor->fp[0], cursor->fp, sizeof(cursor->fp));
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
plframe_error_t plframe_get_reg (plframe_cursor_t *cursor, plframe_regnum_t regnum, plframe_word_t *reg) {
    ucontext_t *uap = cursor->uap;
    
    /* Supported register for this context state? */
    if (!cursor->init_frame) {
        if (regnum == PLFRAME_ARM_PC) {
            *reg = (plframe_word_t) cursor->fp[1];
            return PLFRAME_ESUCCESS;
        }
        
        return PLFRAME_ENOTSUP;
    }
    
    switch (regnum) {
        case PLFRAME_ARM_PC:
            *reg = uap->uc_mcontext->__ss.__pc;
            return PLFRAME_ESUCCESS;
        default:
            return PLFRAME_ENOTSUP;
    }

    return PLFRAME_ENOTSUP;
}


// PLFrameWalker API
plframe_error_t plframe_get_freg (plframe_cursor_t *cursor, plframe_regnum_t regnum, plframe_fpreg_t *fpreg) {
    return PLFRAME_ENOTSUP;
}

#endif /* __arm__ */