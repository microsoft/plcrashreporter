/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008 Plausible Labs Cooperative, Inc.
 * All rights reserved.
 */

#import "GTMSenTestCase.h"

#import "PLCrashFrameWalker.h"


@interface PLCrashFrameWalkerTests : SenTestCase {
@private
    plframe_test_thead_t _thr_args;
}
@end

@implementation PLCrashFrameWalkerTests
    
- (void) setUp {
    plframe_test_thread_spawn(&_thr_args);
}

- (void) tearDown {
    plframe_test_thread_stop(&_thr_args);
}

- (void) testGetRegName {
    for (int i = 0; i < PLFRAME_REG_LAST + 1; i++) {
        const char *name = plframe_get_regname(i);
        STAssertNotNULL(name, @"Register name for %d is NULL", i);
        STAssertNotEquals((size_t)0, strlen(name), @"Register name for %d is 0 length", i);
    }
}

/* test plframe_valid_stackaddr() */
- (void) testReadAddress {
    const char bytes[] = "Hello";
    char dest[sizeof(bytes)];

    // Verify that a good read succeeds
    plframe_read_addr(bytes, dest, sizeof(dest));
    STAssertTrue(strcmp(bytes, dest) == 0, @"Read was not performed");
    
    // Verify that reading off the page at 0x0 fails
    STAssertNotEquals(KERN_SUCCESS, plframe_read_addr(NULL, dest, sizeof(bytes)), @"Bad read was performed");
}


/* test plframe_cursor_init() */
- (void) testInitFrame {
    plframe_cursor_t cursor;

    /* Initialize the cursor */
    STAssertEquals(PLFRAME_ESUCCESS, plframe_cursor_thread_init(&cursor, pthread_mach_thread_np(_thr_args.thread)), @"Initialization failed");

    /* Try fetching the first frame */
    plframe_error_t ferr = plframe_cursor_next(&cursor);
    STAssertEquals(PLFRAME_ESUCCESS, ferr, @"Next failed: %s", plframe_strerror(ferr));
}

@end
