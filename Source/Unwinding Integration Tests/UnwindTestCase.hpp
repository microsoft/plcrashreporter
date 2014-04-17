/*
 * Author: Landon Fuller <landonf@plausible.coop>
 *
 * Copyright (c) 2013-2014 Plausible Labs Cooperative, Inc.
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

#ifndef UNWIND_TEST_H
#define UNWIND_TEST_H 1

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include "UnwindTest.hpp"
#include "PLCrashMacros.h"

namespace plcrash {

/**
 * An unwind test case.
 *
 * @warning The structure layout and semantics must be kept in sync with the assembler macros
 * declared in UnwindTestCase.S.
 */
typedef struct UnwindTestCase {
private:
    /** NUL terminated test case name. */
    const char *_name;

    /** Array of all tests defined for this test case. Must be terminated with a terminal test case (@sa UnwindTest::TERM). */
    const UnwindTest *_tests;

public:
    UnwindTestCase (const char *name, const UnwindTest tests[]);
    
    const char *name (void) const;
    bool executeAll (void) const;
} UnwindTestCase;
    
/********************************************************************************************************
 * TEST CASE DEFINITIONS                                                                                *
 *                                                                                                      *
 * The following symbols are provided by the architecture-specific assembly files.                      *
 *                                                                                                      *
 * The implementation files are named to match the test cases (eg, unwind_test_case_frame_x86_64.S).    *
 ********************************************************************************************************/

/**
 * Tests that maintain a valid frame pointer.
 */
PLCR_C_EXPORT const UnwindTestCase unwind_test_case_frame;

/**
 * Tests that do not maintain a valid frame pointer.
 */
PLCR_C_EXPORT const UnwindTestCase unwind_test_case_frameless;

/**
 * Tests that do not maintain a valid frame pointer, and make use of a large frame size.
 *
 * These tests exercise edge cases such as the UNWIND_X86_MODE_STACK_IND compact unwind encoding,
 * which requires an indirect fetch of a constant stack size too large to be encoded via
 * UNWIND_X86_MODE_STACK_IMMD.
 */
PLCR_C_EXPORT const UnwindTestCase unwind_test_case_frameless_big;

/**
 * Tests that verify "unusual" unwinding behavior.
 *
 * These tests include edge cases that cannot be represented via the compact unwind encoding.
 */
PLCR_C_EXPORT const UnwindTestCase unwind_test_case_frameless_big;
    
} /* namespace plcrash */

#endif /* UNWIND_TEST_H */
