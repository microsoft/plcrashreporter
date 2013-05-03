/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008-2013 Plausible Labs Cooperative, Inc.
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
#import "PLCrashFrameStackUnwind.h"

struct stack_frame {
    uintptr_t fp;
    uintptr_t pc;
} __attribute__((packed));

/**
 * @internal
 *
 * This code tests stack-based frame unwinding. It currently assumes that the stack grows down (which is
 * true on the architectures we currently support, but TODO: this should be direction-neutral).
 */
@interface PLCrashFrameStackUnwindTests : SenTestCase @end

@implementation PLCrashFrameStackUnwindTests

/**
 * Verify that walking terminates with a NULL frame address.
 */
- (void) testNULLFrame {
    /* Set up test stack */
    struct stack_frame frames[] = {
        { .fp = &frames[1], .pc = 0x1 },
        { .fp = &frames[2], .pc = 0x2 },
        { .fp = 0x0,        .pc = 0x3 },
    };
    size_t frame_count = sizeof(frames) / sizeof(frames[0]);

    /* Configure thread state */
    plcrash_async_thread_state_t state;
    plcrash_async_thread_state_mach_thread_init(&state, mach_thread_self());
    plcrash_async_thread_state_set_reg(&state, PLCRASH_REG_FP, frames[0].fp);
    plcrash_async_thread_state_set_reg(&state, PLCRASH_REG_IP, frames[0].pc);

    /* Let the plframe cursor API initialize our first frame */
    plframe_cursor_t cursor;
    plframe_cursor_init(&cursor, mach_task_self(), &state);
    
    /* Try walking the stack */
    plframe_stackframe_t new_frame;
    plframe_stackframe_t frame = cursor.frame;
    for (int i = 0; i < frame_count-1; i++) {
        /* Verify the fetched PC value */
        STAssertTrue(plframe_regset_isset(frame.valid_registers, PLCRASH_REG_IP), @"Did not mark IP as readable");
        plcrash_greg_t pc = plcrash_async_thread_state_get_reg(&frame.thread_state, PLCRASH_REG_IP);
        STAssertEquals(pc, (plcrash_greg_t)frames[i].pc, @"Incorrect IP for index %d", i);

        /* Fetch the next frame */
        STAssertEquals(plframe_cursor_read_frame_ptr(cursor.task, &frame, &new_frame), PLFRAME_ESUCCESS, @"Failed to read next frame");
        frame = new_frame;
    }

    /* Ensure that the final frame's NULL fp triggers an ENOFRAME */
    STAssertEquals(plframe_cursor_read_frame_ptr(cursor.task, &frame, &new_frame), PLFRAME_ENOFRAME, @"Expected to hit end of frames");
}

#if 0
/**
 * Verify that walking terminates with frame address greater than the current address.
 */
- (void) testStackDirection {
    /* Set up test stack */
    struct stack_frame frames[] = {
        { .fp = &frames[1], .pc = 0x1 },
        { .fp = &frames[0], .pc = 0x2 },
        { .fp = &frames[1], .pc = 0x3 },
    };
    size_t frame_count = sizeof(frames) / sizeof(frames[0]);
    
    /* Configure thread state */
    plcrash_async_thread_state_t state;
    plcrash_async_thread_state_mach_thread_init(&state, mach_thread_self());
    plcrash_async_thread_state_set_reg(&state, PLCRASH_REG_FP, frames[0].fp);
    plcrash_async_thread_state_set_reg(&state, PLCRASH_REG_IP, frames[0].pc);
    
    /* Try walking the stack */
    plframe_cursor_t cursor;
    plcrash_greg_t reg;
    
    plframe_cursor_init(&cursor, mach_task_self(), &state);
    
    for (size_t i = 0; i < frame_count-1; i++) {        
        STAssertEquals(plframe_cursor_next_fp(&cursor), PLFRAME_ESUCCESS, @"Failed to step cursor for %d", (int)i);
        STAssertEquals(plframe_cursor_get_reg(&cursor, PLCRASH_REG_IP, &reg), PLFRAME_ESUCCESS, @"Failed to fetch IP");
        STAssertEquals(reg, (plcrash_greg_t)frames[i].pc, @"Incorrect IP");
    }

    /* Ensure that the final frame's bad fp triggers an EBADFRAME */
    STAssertEquals(plframe_cursor_next(&cursor), PLFRAME_EBADFRAME, @"Expected to hit end of frames");
}
#endif

@end
