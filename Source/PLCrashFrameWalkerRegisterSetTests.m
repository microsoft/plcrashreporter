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

#import "PLCrashAsyncThread.h"
#import "PLCrashFrameWalkerRegisterSet.h"

@interface PLCrashFrameWalkerRegisterSetTests : SenTestCase {
@private
    /** The range of registers supported on this platform. */
    size_t _platform_regcount;
    
    /** Register set to use for testing */
    plframe_regset_t _registers;
}
@end

/**
 * Test the register set implementation.
 */
@implementation PLCrashFrameWalkerRegisterSetTests

- (void) setUp {
    /* Determine the valid register range for the current platform */
    plcrash_async_thread_state_t state;
    plcrash_async_thread_state_mach_thread_init(&state, mach_thread_self());
    _platform_regcount = plcrash_async_thread_state_get_reg_count(&state);

    /* Initialize test state */
    plframe_regset_zero(&_registers);
}

/**
 * Test setting of individual register bits. This also verifies that the full platform register set fits within
 * the plframe_regset_t type by iterating over all platform registers.
 */
- (void) testRegisterSet {
    /* Test set/isset. T */
    for (int i = 0; i < _platform_regcount; i++) {
        plframe_regset_set(&_registers, i);
        
        STAssertTrue(plframe_regset_isset(_registers, i), @"Register should be marked as set");
        STAssertEquals(__builtin_popcount(_registers), i+1, @"Incorrect number of 1 bits");
    }
}

- (void) testSetAll {
    /* Set all bits */
    plframe_regset_set_all(&_registers);
    size_t one_count = __builtin_popcount(_registers);
    STAssertEquals(sizeof(_registers) * 8, one_count, @"Did not set all bits to 1");
}

- (void) testUnset {
    /* Set all bits */
    plframe_regset_set_all(&_registers);
    size_t one_count = __builtin_popcount(_registers);
    STAssertEquals(sizeof(_registers) * 8, one_count, @"Did not set all bits to 1");

    /* Test clear */
    plframe_regset_unset(&_registers, 0);
    STAssertFalse(plframe_regset_isset(_registers, 0), @"Register should be marked as unset");
    STAssertEquals((size_t)__builtin_popcount(_registers), one_count-1, @"Incorrect number of 1 bits; more than one register was unset");
}

- (void) testZero {
    plframe_regset_set_all(&_registers);
    plframe_regset_zero(&_registers);
    STAssertEquals((plframe_regset_t)0, _registers, @"Set not zero'd");
}

@end
