//
//  PLCrashLogSystemInfo.h
//  CrashReporter
//
//  Created by Landon Fuller on 12/17/08.
//  Copyright 2008 Plausible Labs Cooperative, Inc.. All rights reserved.
//

#import <Foundation/Foundation.h>

/**
 * @ingroup constants
 *
 * Indicates the Operating System under which a Crash Log was generated.
 *
 * @internal
 * These enum values match the protobuf values. Keep them synchronized.
 */
typedef enum {
    /** Mac OS X. */
    PLCrashLogOperatingSystemMacOSX = 0,
    
    /** iPhone OS */
    PLCrashLogOperatingSystemiPhoneOS = 1,
    
    /** iPhone Simulator (Mac OS X with additional simulator-specific runtime libraries) */
    PLCrashLogOperatingSystemiPhoneSimulator = 2
} PLCrashLogOperatingSystem;

/**
 * @ingroup constants
 *
 * Indicates the architecture under which a Crash Log was generated.
 *
 * @internal
 * These enum values match the protobuf values. Keep them synchronized.
 */
typedef enum {
    /** x86-32. */
    PLCrashLogArchitectureX86_32 = 0,
    
    /** x86-64 */
    PLCrashLogArchitectureX86_64 = 1,
    
    /** ARM */
    PLCrashLogArchitectureARM = 2
} PLCrashLogArchitecture;


extern PLCrashLogOperatingSystem PLCrashLogHostOperatingSystem;
extern PLCrashLogArchitecture PLCrashLogHostArchitecture;

@interface PLCrashLogSystemInfo : NSObject {
@private
    /** Operating system */
    PLCrashLogOperatingSystem _operatingSystem;
    
    /** Operating system version */
    NSString *_osVersion;
    
    /** Architecture */
    PLCrashLogArchitecture _architecture;
    
    /** Date crash report was generated. May be nil if the date is unknown. */
    NSDate *_timestamp;
}

- (id) initWithOperatingSystem: (PLCrashLogOperatingSystem) operatingSystem 
        operatingSystemVersion: (NSString *) operatingSystemVersion
                  architecture: (PLCrashLogArchitecture) architecture
                     timestamp: (NSDate *) timestamp;

/**
 * The operating system.
 */
@property(nonatomic, readonly) PLCrashLogOperatingSystem operatingSystem;

/**
 * The operating system's release version.
 */
@property(nonatomic, readonly) NSString *operatingSystemVersion;

/**
 * Architecture.
 */
@property(nonatomic, readonly) PLCrashLogArchitecture architecture;

/**
 * Date and time that the crash report was generated. This may
 * be unavailable, and this property will be nil.
 */
@property(nonatomic, readonly) NSDate *timestamp;

@end
