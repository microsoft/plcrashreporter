/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008 Plausible Labs Cooperative, Inc.
 * All rights reserved.
 */

#import "PLCrashReportApplicationInfo.h"

/**
 * Crash log application data.
 *
 * Provides the application identifier and version of the crashed
 * application.
 */
@implementation PLCrashReportApplicationInfo

/**
 * Initialize with the provided application identifier and version.
 *
 * @param applicationIdentifier Application identifier. This is usually the CFBundleIdentifier value.
 * @param applicationVersion Application version. This is usually the CFBundleVersion value.
 */
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
