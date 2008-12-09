/*
 *  PLCrashFrameWalker_i386.c
 *  CrashReporter
 *
 *  Created by Landon Fuller on 12/8/08.
 *  Copyright 2008 Plausible Labs Cooperative, Inc.. All rights reserved.
 *
 */

#include "PLCrashFrameWalker.h"

#ifdef __i386__

// PLFrameWalker API
plframe_error_t plframe_cursor_init (plframe_cursor_t *cursor, ucontext_t *uap) {
    cursor->uap = uap;
    cursor->init_frame = true;
    cursor->sp = NULL;

    return PLFRAME_ESUCCESS;
}


// PLFrameWalker API
#include <stdio.h>
plframe_error_t plframe_cursor_next (plframe_cursor_t *cursor) {

    /* Fetch the next stack address */
    if (cursor->sp == NULL) {
        cursor->sp = (void **) cursor->uap->uc_mcontext->__ss.__ebp;
    } else {
        cursor->init_frame = false;
        cursor->sp = cursor->sp[0];
    }
    
    /* Check for completion */
    if (cursor->sp == NULL)
        return PLFRAME_ENOFRAME;

    /* Check for a valid address */
    if (!plframe_valid_stackaddr(cursor->uap, cursor->sp)) {
        return PLFRAME_EBADFRAME;
    }

    /* New frame fetched */
    return PLFRAME_ESUCCESS;
}


// PLFrameWalker API
plframe_error_t plframe_get_reg (plframe_cursor_t *cursor, plframe_regnum_t regnum, plframe_word_t *reg) {
    return PLFRAME_EUNKNOWN;
}


// PLFrameWalker API
plframe_error_t plframe_get_freg (plframe_cursor_t *cursor, plframe_regnum_t regnum, plframe_fpreg_t *fpreg) {
    return PLFRAME_EUNKNOWN;
}

#endif