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

#import <pthread.h>

#import "SenTestCompat.h"

#import "PLCrashFrameWalker.h"
#import "PLCrashTestThread.h"

#import "unwind_test_harness.h"

@interface PLCrashFrameWalkerTests : SenTestCase {
@private
    plcrash_test_thread_t _thr_args;
    plcrash_async_image_list_t *_image_list;
    
    /** The allocator used by our _image_list */
    plcrash_async_allocator_t *_allocator;
}
@end

@implementation PLCrashFrameWalkerTests
    
- (void) setUp {
    /* Set up our allocator */
    STAssertEquals(plcrash_async_allocator_create(&_allocator, PAGE_SIZE*2), PLCRASH_ESUCCESS, @"Failed to create allocator");

    plcrash_test_thread_spawn(&_thr_args);
    
    /* Read our image list */
    STAssertEquals(plcrash_nasync_image_list_new(&_image_list, _allocator, mach_task_self()), PLCRASH_ESUCCESS, @"Failed to create image list");
}

- (void) tearDown {
    plcrash_test_thread_stop(&_thr_args);
    plcrash_async_image_list_free(_image_list);
    
    /* Clean up our allocator (must be done *after* deallocating the _image_list allocated from this allocator) */
    plcrash_async_allocator_free(_allocator);
}

- (void) testGetRegName {
    plframe_cursor_t cursor;
    plframe_cursor_thread_init(&cursor, mach_task_self(), pthread_mach_thread_np(_thr_args.thread), _image_list);

    for (int i = 0; i < plframe_cursor_get_regcount(&cursor); i++) {
        const char *name = plframe_cursor_get_regname(&cursor, i);
        STAssertNotNULL(name, @"Register name for %d is NULL", i);
        STAssertNotEquals((size_t)0, strlen(name), @"Register name for %d is 0 length", i);
    }

    plframe_cursor_free(&cursor);
}

/* test plframe_cursor_init() */
- (void) testInitFrame {
    plframe_cursor_t cursor;

    /* Initialize the cursor */
    STAssertEquals(PLFRAME_ESUCCESS, plframe_cursor_thread_init(&cursor, mach_task_self(), pthread_mach_thread_np(_thr_args.thread), _image_list), @"Initialization failed");

    /* Try fetching the first frame */
    plframe_error_t ferr = plframe_cursor_next(&cursor);
    STAssertEquals(PLFRAME_ESUCCESS, ferr, @"Next failed: %s", plframe_strerror(ferr));

    /* Verify that all registers are supported */
    for (int i = 0; i < plframe_cursor_get_regcount(&cursor); i++) {
        plcrash_greg_t val;
        STAssertEquals(PLFRAME_ESUCCESS, plframe_cursor_get_reg(&cursor, i, &val), @"Could not fetch register value");
    }
}

/* Test-only frame readers */
static plframe_error_t null_ip_reader (task_t task,
                                       plcrash_async_image_list_t *image_list,
                                       const plframe_stackframe_t *current_frame,
                                       const plframe_stackframe_t *previous_frame,
                                       plframe_stackframe_t *next_frame)
{
    plcrash_async_thread_state_copy(&next_frame->thread_state, &current_frame->thread_state);
    plcrash_async_thread_state_set_reg(&next_frame->thread_state, PLCRASH_REG_IP, 0x1);
    return PLFRAME_ESUCCESS;
}

static plframe_error_t esuccess_reader (task_t task,
                                        plcrash_async_image_list_t *image_list,
                                        const plframe_stackframe_t *current_frame,
                                        const plframe_stackframe_t *previous_frame,
                                        plframe_stackframe_t *next_frame)
{
    plcrash_async_thread_state_copy(&next_frame->thread_state, &current_frame->thread_state);
    return PLFRAME_ESUCCESS;
}


/**
 * Test handling of IPs within the NULL page.
 */
- (void) testStep {
    plframe_cursor_t cursor;
    
    /* Initialize the cursor */
    STAssertEquals(PLFRAME_ESUCCESS, plframe_cursor_thread_init(&cursor, mach_task_self(), pthread_mach_thread_np(_thr_args.thread), _image_list), @"Initialization failed");
    
    /* Try fetching the first frame */
    STAssertEquals(PLFRAME_ESUCCESS, plframe_cursor_next(&cursor), @"Failed to fetch first frame");
    
    /* Verify that fetching the next frame fails with ENOFRAME when the reader returns ENOFRAME */
    plframe_cursor_frame_reader_t *readers[] = { null_ip_reader, esuccess_reader };
    STAssertEquals(PLFRAME_ENOFRAME, plframe_cursor_next_with_readers(&cursor, readers, sizeof(readers) / sizeof(readers[0])), @"Did not return expected error code");
    
    plframe_cursor_free(&cursor);
    
}

/*
 * Perform stack walking regression tests.
 */
- (void) testStackWalkerRegression {
    STAssertTrue(unwind_test_harness(), @"Regression tests failed");
}

@end
