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

#ifndef PLCRASH_UNWIND_TEST_CASE_S
#define PLCRASH_UNWIND_TEST_CASE_S 1

#include "UnwindTest.S"

/*
 * Declare an UnwindTestCase header
 *
 * $0 - The test case's symbol name, minus Darwin's leading '_'.
 *
 * Example:
 * UnwindTestCase   unwind_test_frame
 * unwind_test_decl         unwind_test_frame_ebx, 0
 * UnwindTestCaseEnd
 */
.macro UnwindTestCase
    .section __DATA,__const

    /* Add a name for the test case */
    .globl _$0_tc_name
    _$0_tc_name:
        .asciz "$0"

    /* Start the actual test case data structure */
    .globl _$0
    _$0:
        /* _name */
        unwind_ptr _$0_tc_name

        /* Reference to the UnwindTests array */
        unwind_ptr L$0_tc_tests

    L$0_tc_tests:
.endmacro

/*
 * Define an UnwindTest data structure
 *
 * $0 - Pointer to the test's name
 * $1 - Pointer to the test's function
 * $2 - The UnwindTestType value for this test.
 *
 * Example:
 * UnwindTestCase unwind_test_frame
 * unwind_test_decl unwind_test_frame_ebx
 * UnwindTestCaseEnd
 */
.macro _unwind_test_decl_raw
#if __i386__ || __arm__
    unwind_ptr $0
    unwind_ptr $1
    .long $2
#elif __x86_64__ || __arm64__
    unwind_ptr $0
    unwind_ptr $1
    .long $2
    .space 4
#else
#error Select (or add) an appropriate declaration macro for this platform. The default alignment of an existing platform definition may be sufficient.
#endif
.endmacro

/*
 * Include an UnwindTest reference in an UnwindTestCase's list of defined tests.
 *
 * $0 - The test's symbol name, minus Darwin's leading '_' (eg, 'unwind_test_frame_ebx').
 * $1 - The UnwindTestType value for this test.
 *
 * Example:
 * UnwindTestCase unwind_test_frame
 * unwind_test_decl unwind_test_frame_ebx
 * UnwindTestCaseEnd
 */
.macro unwind_test_decl
    _unwind_test_decl_raw _$0_name, _$0, $1
.endmacro

    
/**
 * Declare an UnwindTestCase footer
 */
.macro UnwindTestCaseEnd
    /* Terminate the test entry array using a NULL-initialized UnwindTest. */
    _unwind_test_decl_raw 0, 0, 0
.endmacro



#endif /* PLCRASH_UNWIND_TEST_CASE_S */
