/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008 Plausible Labs Cooperative, Inc.
 * All rights reserved.
 */

#import <Foundation/Foundation.h>

@interface PLCrashReporter : NSObject {
@private
    /** YES if the crash reporter has been enabled */
    BOOL _enabled;

    /** Application identifier */
    NSString *_applicationIdentifier;

    /** Application version */
    NSString *_applicationVersion;

    /** Path to the crash reporter internal data directory */
    NSString *_crashReportDirectory;
}

+ (PLCrashReporter *) sharedReporter;

- (BOOL) enableCrashReporter;
- (BOOL) enableCrashReporterAndReturnError: (NSError **) outError;

@end