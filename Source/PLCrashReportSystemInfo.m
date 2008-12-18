/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008 Plausible Labs Cooperative, Inc.
 * All rights reserved.
 */

#import "PLCrashReportSystemInfo.h"

/**
 * @ingroup constants
 *
 * The current host's operating system.
 */
PLCrashReportOperatingSystem PLCrashReportHostOperatingSystem =
#if TARGET_IPHONE_SIMULATOR
    PLCrashReportOperatingSystemiPhoneSimulator;
#elif TARGET_OS_IPHONE
    PLCrashReportOperatingSystemiPhoneOS;
#elif TARGET_OS_MAC
    PLCrashReportOperatingSystemMacOSX;
#else
    #error Unknown operating system
#endif




/**
 * @ingroup constants
 *
 * The current host's architecture.
 */
PLCrashReportArchitecture PLCrashReportHostArchitecture =
#ifdef __x86_64__
    PLCrashReportArchitectureX86_64;
#elif defined(__i386__)
    PLCrashReportArchitectureX86_32;
#elif defined(__arm__)
    PLCrashReportArchitectureARM;
#else
    #error Unknown machine architecture
#endif


/**
 * Crash log host data.
 *
 * This contains information about the host system, including operating system and architecture.
 */
@implementation PLCrashReportSystemInfo

/**
 * Initialize the system info data object.
 *
 * @param operatingSystem Operating System
 * @param operatingSystemVersion OS version
 * @param architecture Architecture
 * @param timestamp Timestamp (may be nil).
 */
- (id) initWithOperatingSystem: (PLCrashReportOperatingSystem) operatingSystem 
        operatingSystemVersion: (NSString *) operatingSystemVersion
                  architecture: (PLCrashReportArchitecture) architecture
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