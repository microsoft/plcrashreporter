/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008 Plausible Labs Cooperative, Inc.
 * All rights reserved.
 */

#import "PLCrashLogExceptionInfo.h"

/**
 * If a crash is triggered by an uncaught Objective-C exception, the
 * exception name and reason will be made available.
 */
@implementation PLCrashLogExceptionInfo

/**
 * Initialize with the given exception name and reason.
 */
- (id) initWithExceptionName: (NSString *) name reason: (NSString *) reason {
    if ((self = [super init]) == nil)
        return nil;

    _name = [name retain];
    _reason = [reason retain];

    return self;
}

- (void) dealloc {
    [_name release];
    [_reason release];
    [super dealloc];
}

@synthesize exceptionName = _name;
@synthesize exceptionReason = _reason;

@end
