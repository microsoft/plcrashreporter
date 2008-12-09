/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008 Plausible Labs Cooperative, Inc.
 * All rights reserved.
 */

#import "GTMSenTestCase.h"

#import "PLCrashFrameWalker.h"

@interface PLCrashFrameWalkerTests : SenTestCase @end

@implementation PLCrashFrameWalkerTests

/* test plframe_valid_stackaddr() */
- (void) testValidStackAddress {
    ucontext_t uap;

    /* Mock up a stack */
    uap.uc_stack.ss_sp = (uint8_t *) 0x64;
    uap.uc_stack.ss_size = 50;

    /* Test addresses */
    STAssertTrue(plframe_valid_stackaddr(&uap, (uint8_t *) 0x64), @"Valid address");
    STAssertTrue(plframe_valid_stackaddr(&uap, (uint8_t *) 0x65), @"Valid address");
    STAssertTrue(plframe_valid_stackaddr(&uap, (uint8_t *) 0x64 + 49), @"Valid address");
    STAssertFalse(plframe_valid_stackaddr(&uap, (uint8_t *) 0x64 + 50), @"Invalid address");
}

/* test plframe_cursor_init() */
- (void) testInitFrame {
    plframe_cursor_t cursor;

    /* Initialize the cursor */
    STAssertEquals(PLFRAME_ESUCCESS, plframe_cursor_thread_init(&cursor, pthread_mach_thread_np(pthread_self())), @"Initialization failed");

    /* Try fetching our own stack pointer */
    STAssertEquals(PLFRAME_ESUCCESS, plframe_cursor_next(&cursor), @"Next failed");
}

@end
