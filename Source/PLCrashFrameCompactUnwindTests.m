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


#import "SenTestCompat.h"
#import "PLCrashFrameCompactUnwind.h"
#import "PLCrashFeatureConfig.h"

#if PLCRASH_FEATURE_UNWIND_COMPACT

/**
 * @internal
 *
 * This code tests compact frame unwinding.
 */
@interface PLCrashFrameCompactUnwindTests : SenTestCase {
@private
    plcrash_async_image_list_t *_image_list;
    
    /** The allocator used to initialize our _image_list */
    plcrash_async_allocator_t *_allocator;
}

@end

@implementation PLCrashFrameCompactUnwindTests

- (void) setUp {
    /* Set up our allocator */
    STAssertEquals(plcrash_async_allocator_create(&_allocator, PAGE_SIZE*2), PLCRASH_ESUCCESS, @"Failed to create allocator");

    /* Read our image list */
    STAssertEquals(plcrash_nasync_image_list_new(&_image_list, _allocator, mach_task_self()), PLCRASH_ESUCCESS, @"Failed to create image list");
}

- (void) tearDown {
    plcrash_async_image_list_free(_image_list);
    
    /* Clean up our allocator (must be done *after* deallocating the _image_list allocated from this allocator) */
    plcrash_async_allocator_free(_allocator);
}

- (void) testMissingIP {
    plframe_stackframe_t frame;
    plframe_stackframe_t next;
    plframe_error_t err;

    plcrash_async_thread_state_clear_all_regs(&frame.thread_state);
    err = plframe_cursor_read_compact_unwind(mach_task_self(), _image_list, &frame, NULL, &next);
    STAssertEquals(err, PLFRAME_EBADFRAME, @"Unexpected result for a frame missing a valid PC");
}

- (void) testMissingImage {
    plframe_stackframe_t frame;
    plframe_stackframe_t next;
    plframe_error_t err;
    
    plcrash_async_thread_state_clear_all_regs(&frame.thread_state);
    plcrash_async_thread_state_set_reg(&frame.thread_state, PLCRASH_REG_IP, NULL);
    
    err = plframe_cursor_read_compact_unwind(mach_task_self(), _image_list, &frame, NULL, &next);
    STAssertEquals(err, PLFRAME_ENOTSUP, @"Unexpected result for a frame missing a valid image");
}

@end

#endif /* PLCRASH_FEATURE_UNWIND_COMPACT */
