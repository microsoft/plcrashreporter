/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008 Plausible Labs Cooperative, Inc.
 * All rights reserved.
 */

#import "GTMSenTestCase.h"
#import "PLCrashReporter.h"

@interface PLCrashReporterTests : SenTestCase
@end

@implementation PLCrashReporterTests

- (void) testSingleton {
    STAssertNotNil([PLCrashReporter sharedReporter], @"Returned nil singleton instance");
    STAssertTrue([PLCrashReporter sharedReporter] == [[PLCrashReporter alloc] init], @"Crash reporter init method did not return singleton instance");
}

@end
