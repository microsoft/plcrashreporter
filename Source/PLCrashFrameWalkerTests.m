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

#import "GTMSenTestCase.h"

#import "PLCrashFrameWalker.h"
#import "PLCrashTestThread.h"

#import "unwind_test_harness.h"

@interface PLCrashFrameWalkerTests : SenTestCase {
@private
    plcrash_test_thread_t _thr_args;
    plcrash_async_image_list_t _image_list;
}
@end

@implementation PLCrashFrameWalkerTests
    
- (void) setUp {
    plcrash_test_thread_spawn(&_thr_args);
    plcrash_nasync_image_list_init(&_image_list, mach_task_self());
}

- (void) tearDown {
    plcrash_test_thread_stop(&_thr_args);
    plcrash_nasync_image_list_free(&_image_list);
}

- (void) testGetRegName {
    plframe_cursor_t cursor;
    plframe_cursor_thread_init(&cursor, mach_task_self(), pthread_mach_thread_np(_thr_args.thread), &_image_list);

    for (int i = 0; i < plframe_cursor_get_regcount(&cursor); i++) {
        const char *name = plframe_cursor_get_regname(&cursor, i);
        STAssertNotNULL(name, @"Register name for %d is NULL", i);
        STAssertNotEquals((size_t)0, strlen(name), @"Register name for %d is 0 length", i);
    }

    plframe_cursor_free(&cursor);
}

/* Test plframe_thread_state_ucontext_init() */
- (void) testThreadStateContextInit {
    plcrash_async_thread_state_t thr_state;
    pl_mcontext_t mctx;

    memset(&mctx, 'A', sizeof(mctx));

    plcrash_async_thread_state_mcontext_init(&thr_state, &mctx);

#if defined(PLFRAME_ARM_SUPPORT) && defined(__LP64__)
    STAssertTrue(memcmp(&thr_state.arm_state.thread.ts_64, &mctx.__ss, sizeof(thr_state.arm_state.thread.ts_64)) == 0, @"Incorrectly copied");

#elif defined(PLFRAME_ARM_SUPPORT)
    STAssertTrue(memcmp(&thr_state.arm_state.thread.ts_32, &mctx.__ss, sizeof(thr_state.arm_state.thread.ts_32)) == 0, @"Incorrectly copied");

#elif defined(PLFRAME_X86_SUPPORT) && defined(__LP64__)
    STAssertEquals(thr_state.x86_state.thread.tsh.count, (int)x86_THREAD_STATE64_COUNT, @"Incorrect thread state count for a 64-bit system");
    STAssertEquals(thr_state.x86_state.thread.tsh.flavor, (int)x86_THREAD_STATE64, @"Incorrect thread state flavor for a 64-bit system");
    STAssertTrue(memcmp(&thr_state.x86_state.thread.uts.ts64, &mctx.__ss, sizeof(thr_state.x86_state.thread.uts.ts64)) == 0, @"Incorrectly copied");

    STAssertEquals(thr_state.x86_state.exception.esh.count, (int) x86_EXCEPTION_STATE64_COUNT, @"Incorrect thread state count for a 64-bit system");
    STAssertEquals(thr_state.x86_state.exception.esh.flavor, (int) x86_EXCEPTION_STATE64, @"Incorrect thread state flavor for a 64-bit system");
    STAssertTrue(memcmp(&thr_state.x86_state.exception.ues.es64, &mctx.__es, sizeof(thr_state.x86_state.exception.ues.es64)) == 0, @"Incorrectly copied");
#elif defined(PLFRAME_X86_SUPPORT)
    STAssertEquals(thr_state.x86_state.thread.tsh.count, (int)x86_THREAD_STATE32_COUNT, @"Incorrect thread state count for a 32-bit system");
    STAssertEquals(thr_state.x86_state.thread.tsh.flavor, (int)x86_THREAD_STATE32, @"Incorrect thread state flavor for a 32-bit system");
    STAssertTrue(memcmp(&thr_state.x86_state.thread.uts.ts32, &mctx.__ss, sizeof(thr_state.x86_state.thread.uts.ts32)) == 0, @"Incorrectly copied");

    STAssertEquals(thr_state.x86_state.exception.esh.count, (int)x86_EXCEPTION_STATE32_COUNT, @"Incorrect thread state count for a 32-bit system");
    STAssertEquals(thr_state.x86_state.exception.esh.flavor, (int)x86_EXCEPTION_STATE32, @"Incorrect thread state flavor for a 32-bit system");
    STAssertTrue(memcmp(&thr_state.x86_state.exception.ues.es32, &mctx.__es, sizeof(thr_state.x86_state.exception.ues.es32)) == 0, @"Incorrectly copied");
#else
#error Add platform support
#endif
}

/* Test plframe_thread_state_thread_init() */
- (void) testThreadStateThreadInit {
    plcrash_async_thread_state_t thr_state;
    mach_msg_type_number_t state_count;
    thread_t thr;

    /* Spawn a test thread */
    thr = pthread_mach_thread_np(_thr_args.thread);
    thread_suspend(thr);

    /* Fetch the thread state */
    STAssertEquals(plcrash_async_thread_state_mach_thread_init(&thr_state, thr), PLFRAME_ESUCCESS, @"Failed to initialize thread state");

    /* Test the results */
#if defined(PLFRAME_ARM_SUPPORT)
    arm_thread_state_t local_thr_state;
    state_count = ARM_THREAD_STATE_COUNT;

    STAssertEquals(thread_get_state(thr, ARM_THREAD_STATE, (thread_state_t) &local_thr_state, &state_count), KERN_SUCCESS, @"Failed to fetch thread state");
    STAssertTrue(memcmp(&thr_state.arm_state.thread, &local_thr_state, sizeof(thr_state.arm_state.thread)) == 0, @"Incorrectly copied");

#elif defined(PLFRAME_X86_SUPPORT) && defined(__LP64__)
    state_count = x86_THREAD_STATE64_COUNT;
    x86_thread_state64_t local_thr_state;
    STAssertEquals(thread_get_state(thr, x86_THREAD_STATE64, (thread_state_t) &local_thr_state, &state_count), KERN_SUCCESS, @"Failed to fetch thread state");
    STAssertTrue(memcmp(&thr_state.x86_state.thread.uts.ts64, &local_thr_state, sizeof(thr_state.x86_state.thread.uts.ts64)) == 0, @"Incorrectly copied");
    STAssertEquals(thr_state.x86_state.thread.tsh.count, (int)x86_THREAD_STATE64_COUNT, @"Incorrect thread state count for a 64-bit system");
    STAssertEquals(thr_state.x86_state.thread.tsh.flavor, (int)x86_THREAD_STATE64, @"Incorrect thread state flavor for a 64-bit system");

    state_count = x86_EXCEPTION_STATE64_COUNT;
    x86_exception_state64_t local_exc_state;
    STAssertEquals(thread_get_state(thr, x86_EXCEPTION_STATE64, (thread_state_t) &local_exc_state, &state_count), KERN_SUCCESS, @"Failed to fetch thread state");
    STAssertTrue(memcmp(&thr_state.x86_state.exception.ues.es64, &local_exc_state, sizeof(thr_state.x86_state.exception.ues.es64)) == 0, @"Incorrectly copied");
    STAssertEquals(thr_state.x86_state.exception.esh.count, (int) x86_EXCEPTION_STATE64_COUNT, @"Incorrect thread state count for a 64-bit system");
    STAssertEquals(thr_state.x86_state.exception.esh.flavor, (int) x86_EXCEPTION_STATE64, @"Incorrect thread state flavor for a 64-bit system");

#elif defined(PLFRAME_X86_SUPPORT)
    state_count = x86_THREAD_STATE32_COUNT;
    x86_thread_state32_t local_thr_state;
    STAssertEquals(thread_get_state(thr, x86_THREAD_STATE32, (thread_state_t) &local_thr_state, &state_count), KERN_SUCCESS, @"Failed to fetch thread state");
    STAssertTrue(memcmp(&thr_state.x86_state.thread.uts.ts32, &local_thr_state, sizeof(thr_state.x86_state.thread.uts.ts32)) == 0, @"Incorrectly copied");
    STAssertEquals(thr_state.x86_state.thread.tsh.count, (int)x86_THREAD_STATE32_COUNT, @"Incorrect thread state count for a 64-bit system");
    STAssertEquals(thr_state.x86_state.thread.tsh.flavor, (int)x86_THREAD_STATE32, @"Incorrect thread state flavor for a 32-bit system");

    state_count = x86_EXCEPTION_STATE32_COUNT;
    x86_exception_state32_t local_exc_state;
    STAssertEquals(thread_get_state(thr, x86_EXCEPTION_STATE32, (thread_state_t) &local_exc_state, &state_count), KERN_SUCCESS, @"Failed to fetch thread state");
    STAssertTrue(memcmp(&thr_state.x86_state.exception.ues.es32, &local_exc_state, sizeof(thr_state.x86_state.exception.ues.es32)) == 0, @"Incorrectly copied");
    STAssertEquals(thr_state.x86_state.exception.esh.count, (int) x86_EXCEPTION_STATE32_COUNT, @"Incorrect thread state count for a 32-bit system");
    STAssertEquals(thr_state.x86_state.exception.esh.flavor, (int) x86_EXCEPTION_STATE32, @"Incorrect thread state flavor for a 32-bit system");
#else
#error Add platform support
#endif

    /* Clean up */
    thread_resume(thr);
}

/* test plframe_cursor_init() */
- (void) testInitFrame {
    plframe_cursor_t cursor;

    /* Initialize the cursor */
    STAssertEquals(PLFRAME_ESUCCESS, plframe_cursor_thread_init(&cursor, mach_task_self(), pthread_mach_thread_np(_thr_args.thread), &_image_list), @"Initialization failed");

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
    STAssertEquals(PLFRAME_ESUCCESS, plframe_cursor_thread_init(&cursor, mach_task_self(), pthread_mach_thread_np(_thr_args.thread), &_image_list), @"Initialization failed");
    
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
