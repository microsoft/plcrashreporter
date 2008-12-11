/*
 *  PLCrashAsync.h
 *  CrashReporter
 *
 *  Created by Landon Fuller on 12/10/08.
 *  Copyright 2008 Plausible Labs Cooperative, Inc.. All rights reserved.
 *
 */


#import <stdio.h> // for snprintf
#import <unistd.h>

/**
 * @internal
 * @defgroup plcrash_async Async Safe Utilities
 * @ingroup plcrash_internal
 *
 * Implements async-safe utility functions
 *
 * @{
 */

// Debug output support. Lines are capped at 1024
#define PLCF_DEBUG(msg, args...) {\
    char output[1024];\
    snprintf(output, sizeof(output), "[PLCrashReport] " msg "\n", ## args); \
    write(STDERR_FILENO, output, strlen(output));\
}


/**
 * @} plcrash_async
 */