/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008 Plausible Labs Cooperative, Inc.
 * All rights reserved.
 */


#include "PLCrashFrameWalker.h"

/**
 * Return an error description for the given plframe_error_t.
 */
const char *plframe_strerror (plframe_error_t error) {
    switch (error) {
        case PLFRAME_ESUCCESS:
            return "No error";
        case PLFRAME_EUNKNOWN:
            return "Unknown error";
        case PLFRAME_ENOFRAME:
            return "No frames are available";
        case PLFRAME_EBADFRAME:
            return "Corrupted frame";
        case PLFRAME_EINVAL:
            return "Invalid argument";
        case PLFRAME_EBADREG:
            return "Invalid register";
    }

    /* Should be unreachable */
    return "Unhandled error code";
}

/* Return true if the given address is a valid stack address for the @a uap thread context */
bool plframe_valid_stackaddr (ucontext_t *uap, void *addr) {
#if (PLFRAME_STACK_DIRECTION == PLFRAME_STACK_DIR_DOWN)
    if (uap->uc_stack.ss_sp >= addr && addr >= uap->uc_stack.ss_sp - uap->uc_stack.ss_size)
        return true;
#else
    if (uap->uc_stack.ss_sp <= addr && addr < uap->uc_stack.ss_sp + uap->uc_stack.ss_size)
        return true;
#endif // PLFRAME_STACK_DIRECTION

    return false;
}
