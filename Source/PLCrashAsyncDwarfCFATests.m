/*
 * Author: Landon Fuller <landonf@plausible.coop>
 *
 * Copyright (c) 2012-2013 Plausible Labs Cooperative, Inc.
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

#import "PLCrashTestCase.h"

#include "PLCrashAsyncDwarfCFA.h"

@interface PLCrashAsyncDwarfCFATests : PLCrashTestCase {
    plcrash_async_dwarf_gnueh_ptr_state_t _ptr_state;
    plcrash_async_dwarf_cie_info_t _cie;
}
@end

/**
 * Test DWARF CFA interpretation.
 */
@implementation PLCrashAsyncDwarfCFATests

- (void) setUp {
    /* Initialize required configuration for pointer dereferencing */
    plcrash_async_dwarf_gnueh_ptr_state_init(&_ptr_state, 4);

    _cie.segment_size = 0x0; // we don't use segments
    _cie.has_eh_augmentation = true;
    _cie.eh_augmentation.has_pointer_encoding = true;
    _cie.eh_augmentation.pointer_encoding = DW_EH_PE_absptr; // direct pointers
    _cie.address_size = _ptr_state.address_size;
}

- (void) tearDown {
    plcrash_async_dwarf_gnueh_ptr_state_free(&_ptr_state);
}

/* Perform evaluation of the given opcodes, expecting a result of type @a type,
 * with an expected value of @a expected. The data is interpreted as big endian. */
#define PERFORM_EVAL_TEST(opcodes) do { \
    plcrash_async_mobject_t mobj; \
    plcrash_error_t err; \
    STAssertEquals(PLCRASH_ESUCCESS, plcrash_async_mobject_init(&mobj, mach_task_self(), &opcodes, sizeof(opcodes), true), @"Failed to initialize mobj"); \
    \
        err = plcrash_async_dwarf_eval_cfa_program(&mobj, &_cie, &_ptr_state, plcrash_async_byteorder_big_endian(), &opcodes, 0, sizeof(opcodes)); \
        STAssertEquals(err, PLCRASH_ESUCCESS, @"Evaluation failed"); \
    \
    plcrash_async_mobject_free(&mobj); \
} while(0)

/** Test evaluation of DW_CFA_set_loc */
- (void) testSetLoc {
    uint8_t opcodes[] = { DW_CFA_set_loc, 0x1, 0x2, 0x3, 0x4 };
    PERFORM_EVAL_TEST(opcodes);
}

/** Test evaluation of DW_CFA_set_loc without GNU EH agumentation data (eg, using direct word sized pointers) */
- (void) testSetLocDirect {
    _cie.has_eh_augmentation = false;
    uint8_t opcodes[] = { DW_CFA_set_loc, 0x1, 0x2, 0x3, 0x4 };
    PERFORM_EVAL_TEST(opcodes);
}

/** Test basic evaluation of a NOP. */
- (void) testNop {
    uint8_t opcodes[] = { DW_CFA_nop, };
    
    PERFORM_EVAL_TEST(opcodes);
}

@end