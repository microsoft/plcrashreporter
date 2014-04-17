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

#define UNWIND_TEST_CONFIG_NON_ASSEMBLER_IMPORT 1
#include "unwind_test_config.h"
#undef UNWIND_TEST_CONFIG_NON_ASSEMBLER_IMPORT

/**
 * A set of bit flags defining what unwind data is available for the given
 * test function. The following flags are supported; refer to the individual
 * flag's documentation for more details:
 *
 * - UNWIND_TEST_NO_COMPACT_UNWIND
 * - UNWIND_TEST_NO_DWARF
 * - UNWIND_TEST_NO_FRAME
 */
typedef uint32_t unwind_test_flags_t;

/**
 * An unwind test function declaration. Each unwind test case (frame, frameless, unusual, etc ...)
 * declares a set of unwind test functions. The test function declarations are used to determine
 * which unwinders to test for a given set of unwind functions.
 */
typedef struct {
    /** The test function to be called */
    void (*test_function)(void *context);
    
    /** Test configuration flags */
    unwind_test_flags_t flags;
} unwind_test_entry_t;

/**
 * An unwind test case declaration. This array must be terminated with an
 * entry conaining a NULL unwind_test_entry_t::test_function
 */
typedef unwind_test_entry_t unwind_test_case_t[];

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
extern unwind_test_case_t unwind_test_case_frame;

/**
 * Tests that do not maintain a valid frame pointer.
 */
extern unwind_test_case_t unwind_test_case_frameless;

/**
 * Tests that do not maintain a valid frame pointer, and make use of a large frame size.
 *
 * These tests exercise edge cases such as the UNWIND_X86_MODE_STACK_IND compact unwind encoding,
 * which requires an indirect fetch of a constant stack size too large to be encoded via
 * UNWIND_X86_MODE_STACK_IMMD.
 */
extern unwind_test_case_t unwind_test_case_frameless_big;

/**
 * Tests that verify "unusual" unwinding behavior.
 *
 * These tests include edge cases that cannot be represented via the compact unwind encoding.
 */
extern unwind_test_case_t unwind_test_case_frameless_big;

#endif /* UNWIND_TEST_H */
