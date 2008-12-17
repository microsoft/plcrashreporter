/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008 Plausible Labs Cooperative, Inc.
 * All rights reserved.
 */

#import "PLCrashLogSystemInfo.h"

/**
 * @ingroup constants
 *
 * The current host's operating system.
 */
PLCrashLogOperatingSystem PLCrashLogHostOperatingSystem =
#if TARGET_IPHONE_SIMULATOR
    PLCrashLogOperatingSystemiPhoneSimulator;
#elif TARGET_OS_IPHONE
    PLCrashLogOperatingSystemiPhoneSimulator;
#elif TARGET_OS_MAC
    PLCrashLogOperatingSystemMacOSX;
#else
    #error Unknown operating system
#endif




/**
 * @ingroup constants
 *
 * The current host's architecture.
 */
PLCrashLogArchitecture PLCrashLogHostArchitecture =
#ifdef __x86_64__
    PLCrashLogArchitectureX86_64;
#elif defined(__i386__)
    PLCrashLogArchitectureX86_32;
#elif defined(__arm__)
    PLCrashLogArchitectureARM;
#else
    #error Unknown machine architecture
#endif


/**
 * Crash log host data.
 *
 * This contains information about the host system, including operating system and architecture.
 */
@implementation PLCrashLogSystemInfo

/**
 * Initialize the system info data object.
 *
 * @param operatingSystem Operating System
 * @param operatingSystemVersion OS version
 * @param architecture Architecture
 * @param timestamp Timestamp (may be nil).
 */
- (id) initWithOperatingSystem: (PLCrashLogOperatingSystem) operatingSystem 
        operatingSystemVersion: (NSString *) operatingSystemVersion
                  architecture: (PLCrashLogArchitecture) architecture
                     timestamp: (NSDate *) timestamp
{
    if ((self = [super init]) == nil)
        return nil;
    
    _operatingSystem = operatingSystem;
    _osVersion = [operatingSystemVersion retain];
    _architecture = architecture;
    _timestamp = [timestamp retain];
    
    return self;
}

- (void) dealloc {
    [_osVersion release];
    [_timestamp release];
    [super dealloc];
}

@synthesize operatingSystem = _operatingSystem;
@synthesize operatingSystemVersion = _osVersion;
@synthesize architecture = _architecture;
@synthesize timestamp = _timestamp;

@end