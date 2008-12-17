/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008 Plausible Labs Cooperative, Inc.
 * All rights reserved.
 */

#import "PLCrashLogApplicationInfo.h"


@implementation PLCrashLogApplicationInfo

- (id) initWithApplicationIdentifier: (NSString *) applicationIdentifier 
                  applicationVersion: (NSString *) applicationVersion
{
    if ((self = [super init]) == nil)
        return nil;

    _applicationIdentifier = [applicationIdentifier retain];
    _applicationVersion = [applicationVersion retain];

    return self;
}

- (void) dealloc {
    [_applicationIdentifier release];
    [_applicationVersion release];
    [super dealloc];
}

@synthesize applicationIdentifier = _applicationIdentifier;
@synthesize applicationVersion = _applicationVersion;

@end
