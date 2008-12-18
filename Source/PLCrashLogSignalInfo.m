/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008 Plausible Labs Cooperative, Inc.
 * All rights reserved.
 */

#import "PLCrashLogSignalInfo.h"


/**
 * Provides access to the signal name and signal code.
 */
@implementation PLCrashLogSignalInfo

/**
 * Initialize with the given signal name and reason.
 */
- (id) initWithSignalName: (NSString *) name code: (NSString *) code {
    if ((self = [super init]) == nil)
        return nil;
    
    _name = [name retain];
    _code = [code retain];
    
    return self;
}

- (void) dealloc {
    [_name release];
    [_code release];
    [super dealloc];
}

@synthesize name = _name;
@synthesize code = _code;

@end
