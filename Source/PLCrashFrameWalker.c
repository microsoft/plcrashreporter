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
        case PLFRAME_ENOTSUP:
            return "Operation not supported";
        case PLFRAME_EINVAL:
            return "Invalid argument";
        case PLFRAME_INTERNAL:
            return "Internal error";
        case PLFRAME_EBADREG:
            return "Invalid register";
    }

    /* Should be unreachable */
    return "Unhandled error code";
}

/* Return true if the given address is a valid and readable. */
bool plframe_valid_addr (void *addr, size_t len) {
    kern_return_t kr;
    intptr_t data[len];

    vm_size_t read_size = sizeof(data);
    kr = vm_read_overwrite(mach_task_self(), (vm_address_t) addr, sizeof(data), (pointer_t) data, &read_size);
    if (kr == KERN_SUCCESS) {
        return true;
    }

    return false;
}
