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
#if (PLFRAME_STACK_DIRECTION == PLFRAME_STACK_DIR_DOWN)
    STAssertTrue(plframe_valid_stackaddr(&uap, (uint8_t *) 0x64), @"Valid address");
    STAssertTrue(plframe_valid_stackaddr(&uap, (uint8_t *) 0x63), @"Valid address");
    STAssertTrue(plframe_valid_stackaddr(&uap, (uint8_t *) 0x64 - 50), @"Valid address");
    STAssertFalse(plframe_valid_stackaddr(&uap, (uint8_t *) 0x64 - 51), @"Invalid address");
#else
    STAssertTrue(plframe_valid_stackaddr(&uap, (uint8_t *) 0x64), @"Valid address");
    STAssertTrue(plframe_valid_stackaddr(&uap, (uint8_t *) 0x65), @"Valid address");
    STAssertTrue(plframe_valid_stackaddr(&uap, (uint8_t *) 0x64 + 49), @"Valid address");
    STAssertFalse(plframe_valid_stackaddr(&uap, (uint8_t *) 0x64 + 50), @"Invalid address");
#endif
}

/* test plframe_cursor_init() */
- (void) testInitFrame {
    ucontext_t *uap;
    plframe_cursor_t cursor;

    /* Fetch the current thread context */
    uap = malloc(sizeof(uap));
    getcontext(uap);

    /* Initialize the cursor */
    plframe_cursor_init(&cursor, uap);

    /* Clean up */
    free(uap);
}

@end
