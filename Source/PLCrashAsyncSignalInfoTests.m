/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008 Plausible Labs Cooperative, Inc.
 * All rights reserved.
 */

#import "GTMSenTestCase.h"

#import "PLCrashAsyncSignalInfo.h"

@interface PLCrashAsyncSignalInfoTests : SenTestCase @end


@implementation PLCrashAsyncSignalInfoTests

- (void) testInvalidMapping {
    STAssertNULL(plcrash_async_signal_sigcode(SIGIOT, 42), @"Invalid signal/code should return NULL");
}

- (void) testValidMapping {
    STAssertTrue(strcmp(plcrash_async_signal_sigcode(SIGSEGV, SEGV_NOOP), "SEGV_NOOP") == 0, @"Incorrect mapping performed");
}


@end
