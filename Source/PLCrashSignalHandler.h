/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008 Plausible Labs Cooperative, Inc.
 * All rights reserved.
 */

#import <Foundation/Foundation.h>

@interface PLCrashSignalHandler : NSObject {
@private
    /** Signal stack */
    stack_t _sigstk;
}

/**
 * @internal
 * Signal handler callback.
 */
typedef void (*PLCrashSignalHandlerCallback)(int signal, siginfo_t *info, ucontext_t *uap, void *context);

+ (PLCrashSignalHandler *) sharedHandler;
- (BOOL) registerHandlerWithCallback: (PLCrashSignalHandlerCallback) crashCallback context: (void *) context error: (NSError **) outError;

@end
