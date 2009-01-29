/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008-2009 Plausible Labs Cooperative, Inc.
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
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

    /* Verify that all registers are supported */
    for (int i = 0; i < PLFRAME_REG_LAST + 1; i++) {
        plframe_greg_t val;
        STAssertEquals(PLFRAME_ESUCCESS, plframe_get_reg(&cursor, i, &val), @"Could not fetch register value");
    }
}

@end
