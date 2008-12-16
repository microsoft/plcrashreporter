/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008 Plausible Labs Cooperative, Inc.
 * All rights reserved.
 */

#import "PLCrashReporter.h"

/**
 * @internal
 *
 * Crash reporter singleton. Must default to NULL,
 * or the -[PLCrashReporter init] method will fail when attempting
 * to detect if another crash reporter exists.
 */
static PLCrashReporter *sharedReporter = NULL;

/**
 * Shared application crash reporter.
 */
@implementation PLCrashReporter

/* Create the shared crash reporter singleton. */
+ (void) initialize {
    sharedReporter = NULL;
    sharedReporter = [[PLCrashReporter alloc] init];
}

/**
 * Return the application's crash reporter instance.
 */
+ (PLCrashReporter *) sharedReporter {
    return sharedReporter;
}


/**
 * @internal
 *
 * This is the designated class initializer, but it is not intended
 * to be called externally. If 
 */
- (id) init {
    /* Only allow one instance to be created, no matter what */
    if (sharedReporter != NULL) {
        [self release];
        return sharedReporter;
    }

    /* Initialize our superclass */
    if ((self = [super init]) == nil)
        return nil;

    return self;
}

@end
