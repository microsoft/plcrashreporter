/*
 *  PLCrashFrameWalker_arm.c
 *  CrashReporter
 *
 *  Created by Landon Fuller on 12/8/08.
 *  Copyright 2008 Plausible Labs Cooperative, Inc.. All rights reserved.
 *
 */

#include "PLCrashFrameWalker.h"

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
    return PLFRAME_ENOTSUP;
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