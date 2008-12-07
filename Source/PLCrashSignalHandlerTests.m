/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008 Plausible Labs Cooperative, Inc.
 * All rights reserved.
 */

#import "GTMSenTestCase.h"

#import "PLCrashReporter.h"
#import "PLCrashSignalHandler.h"

@interface PLCrashSignalHandlerTests : SenTestCase @end

@implementation PLCrashSignalHandlerTests

- (void) testSharedHandler {
    STAssertNotNil([PLCrashSignalHandler sharedHandler], @"Nil shared handler");
}

- (void) testDoubleRegister {
    NSError *error = nil;

    STAssertTrue([[PLCrashSignalHandler sharedHandler] registerHandlerAndReturnError: &error], @"Registration failed: %@", error);
    STAssertThrows([[PLCrashSignalHandler sharedHandler] registerHandlerAndReturnError: &error], @"Second registration should throw an exception");
}


@end
