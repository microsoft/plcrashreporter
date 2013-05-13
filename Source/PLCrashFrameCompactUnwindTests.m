/*
 * Copyright (c) 2013 Plausible Labs Cooperative, Inc.
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
#import "PLCrashFrameCompactUnwind.h"

/**
 * @internal
 *
 * This code tests compact frame unwinding.
 */
@interface PLCrashFrameCompactUnwindTests : SenTestCase {
@private
    plcrash_async_image_list_t _image_list;
}

@end

@implementation PLCrashFrameCompactUnwindTests

- (void) setUp {
    plcrash_nasync_image_list_init(&_image_list, mach_task_self());
}

- (void) tearDown {
    plcrash_nasync_image_list_free(&_image_list);
}

- (void) testMissingIP {
    plframe_stackframe_t frame;
    plframe_stackframe_t next;
    plframe_error_t err;

    plcrash_async_thread_state_clear_all_regs(&frame.thread_state);
    err = plframe_cursor_read_compact_unwind(mach_task_self(), &_image_list, &frame, NULL, &next);
    STAssertEquals(err, PLFRAME_EBADFRAME, @"Unexpected result for a frame missing a valid PC");
}

- (void) testMissingImage {
    plframe_stackframe_t frame;
    plframe_stackframe_t next;
    plframe_error_t err;
    
    plcrash_async_thread_state_clear_all_regs(&frame.thread_state);
    plcrash_async_thread_state_set_reg(&frame.thread_state, PLCRASH_REG_IP, NULL);
    
    err = plframe_cursor_read_compact_unwind(mach_task_self(), &_image_list, &frame, NULL, &next);
    STAssertEquals(err, PLFRAME_ENOTSUP, @"Unexpected result for a frame missing a valid image");
}

#define INSERT_BITS(bits, mask) ((bits << __builtin_ctz(mask)) & mask)

- (void) testApplyFramePTRState {
    plcrash_async_cfe_entry_t entry;
    plcrash_async_thread_state_t ts;
    
    /* Create a frame encoding, with registers saved at rbp-1020 bytes */
    const uint32_t encoded_reg_rbp_offset = 1016;
    const uint32_t encoded_regs = UNWIND_X86_64_REG_R12 |
    (UNWIND_X86_64_REG_R13 << 3) |
    (UNWIND_X86_64_REG_R14 << 6);
    
    uint32_t encoding = UNWIND_X86_64_MODE_RBP_FRAME |
    INSERT_BITS(encoded_reg_rbp_offset/8, UNWIND_X86_64_RBP_FRAME_OFFSET) |
    INSERT_BITS(encoded_regs, UNWIND_X86_64_RBP_FRAME_REGISTERS);

    STAssertEquals(PLCRASH_ESUCCESS, plcrash_async_cfe_entry_init(&entry, CPU_TYPE_X86_64, encoding), @"Failed to initialize CFE entry");

    /* Initialize default thread state */
    plcrash_async_thread_state_mach_thread_init(&ts, mach_thread_self());
    plcrash_async_thread_state_clear_all_regs(&ts);
    
    /* Apply! */
    plcrash_async_thread_state_t nts;
    plframe_error_t err = plframe_cursor_apply_compact_unwind(mach_task_self(), &ts, &entry, &nts);
    STAssertEquals(err, PLFRAME_ESUCCESS, @"Failed to apply state to thread");
}

@end
