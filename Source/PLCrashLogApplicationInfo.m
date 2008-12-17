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

    _identifier = [applicationIdentifier retain];
    _version = [applicationVersion retain];

    return self;
}

- (void) dealloc {
    [_identifier release];
    [_version release];
    [super dealloc];
}

@synthesize identifier = _identifier;
@synthesize version = _version;

@end
