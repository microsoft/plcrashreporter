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

// PLFrameWalker API
plframe_error_t plframe_cursor_init (plframe_cursor_t *cursor, ucontext_t *uap) {
    cursor->uap = uap;
    cursor->init_frame = true;
    cursor->fp = NULL;
    
    return PLFRAME_ESUCCESS;
}


// PLFrameWalker API
plframe_error_t plframe_cursor_next (plframe_cursor_t *cursor) {
    /* Fetch the next stack address */
    if (cursor->fp == NULL) {
        cursor->fp = (void **) cursor->uap->uc_mcontext->__ss.__r[7]; // Apple uses r7 as the frame pointer register
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
    if (cursor->init_frame) {
        if (regnum == PLFRAME_ARM_PC) {
            *reg = uap->uc_mcontext->__ss.__pc;
            return PLFRAME_ESUCCESS;
        }
        
        return PLFRAME_ENOTSUP;
    }

    return PLFRAME_ENOTSUP;
}


// PLFrameWalker API
plframe_error_t plframe_get_freg (plframe_cursor_t *cursor, plframe_regnum_t regnum, plframe_fpreg_t *fpreg) {
    return PLFRAME_ENOTSUP;
}

#endif /* __arm__ */