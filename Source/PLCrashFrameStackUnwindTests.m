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
    uintptr_t lr;
} __attribute__((packed));

@interface PLCrashFrameStackUnwindTests : SenTestCase @end

@implementation PLCrashFrameStackUnwindTests

/**
 * Initialize @a thread_state with the given @a fp and @pc
 *
 * @param thread_state The thread state to be initialized.
 * @param fp The thread's initial frame pointer.
 * @param pc The thread's initial program counter.
 *
 * @todo Investigating lifting this out into a generic plframe_cursor_set_reg() API.
 */
- (void) initializeThreadStackState: (plframe_thread_state_t *) thread_state framePointer: (uintptr_t) fp programCounter: (uintptr_t) pc {
    memset(thread_state, 0, sizeof(*thread_state));

#ifdef __arm__
    arm_thread_state_t *arm = &thread_state->arm_state.thread;
    arm->__r[7] = fp;
    arm->__pc = pc;
#elif defined(__x86_64__)
    thread_state->x86_state.thread.tsh.flavor = x86_THREAD_STATE64;
    thread_state->x86_state.thread.tsh.count = x86_THREAD_STATE64_COUNT;

    x86_thread_state64_t *x64 = &thread_state->x86_state.thread.uts.ts64;
    x64->__rbp = fp;
    x64->__rip = pc;
#elif defined(__i386__)
    thread_state->x86_state.thread.tsh.flavor = x86_THREAD_STATE32;
    thread_state->x86_state.thread.tsh.count = x86_THREAD_STATE32_COUNT;

    x86_thread_state32_t *x32 = &thread_state->x86_state.thread.uts.ts32;
    x32->__ebp = fp;
    x32->__eip = pc;
#else
#error Add platform support
#endif /* __arm__ */
}

/**
 * Verify that walking terminates with a NULL frame address.
 */
- (void) testNULLFrame {
    /* Set up test stack */
    struct stack_frame frames[] = {
        { .fp = 0x0,        .lr = 0x1 },
        { .fp = &frames[0], .lr = 0x1 },
        { .fp = &frames[1], .lr = 0x1 },
    };
    size_t frame_count = sizeof(frames) / sizeof(frames[0]);

    frames[0].fp = 0x0;
    frames[0].lr = 0x1;

    frames[1].fp = &frames[0];
    frames[1].lr = 0x2;
    
    frames[2].fp = &frames[1];
    frames[2].lr = 0x3;

    /* Test thread */
    plframe_thread_state_t state;
    [self initializeThreadStackState: &state
                        framePointer: frames[frame_count-1].fp
                      programCounter: frames[frame_count-1].lr];

    /* Try walking the stack */
    plframe_cursor_t cursor;
    plframe_greg_t reg;

    plframe_cursor_init(&cursor, mach_task_self(), &state);
    
    for (size_t i = 0; i < frame_count-1; i++) {
        size_t idx = frame_count-i-1;

        STAssertEquals(plframe_cursor_next_fp(&cursor), PLFRAME_ESUCCESS, @"Failed to step cursor");
        STAssertEquals(plframe_cursor_get_reg(&cursor, PLFRAME_REG_IP, &reg), PLFRAME_ESUCCESS, @"Failed to fetch IP");
        STAssertEquals(reg, (plframe_greg_t)frames[idx].lr, @"Incorrect IP");
    }
    
    /* Ensure that the final frame's NULL fp triggers an ENOFRAME */
    STAssertEquals(plframe_cursor_next(&cursor), PLFRAME_ENOFRAME, @"Expected to hit end of frames");
}

@end
