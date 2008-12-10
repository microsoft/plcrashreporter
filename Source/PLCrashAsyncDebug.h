/*
 *  PLCrashAsyncDebug.h
 *  CrashReporter
 *
 *  Created by Landon Fuller on 12/10/08.
 *  Copyright 2008 Plausible Labs Cooperative, Inc.. All rights reserved.
 *
 */

#import <stdio.h> // for snprintf
#import <unistd.h>

// A super simple debug 'function' that's async safe.
#define PLCF_DEBUG(msg, args...) {\
    char output[1024];\
    snprintf(output, sizeof(output), "[PLCrashReport] " msg "\n", ## args); \
    write(STDERR_FILENO, output, strlen(output));\
}
