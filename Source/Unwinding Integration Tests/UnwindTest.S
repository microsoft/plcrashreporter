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

#ifndef PLCRASH_UNWIND_TEST_S
#define PLCRASH_UNWIND_TEST_S 1

/* The PLCRASH_UNWIND_TEST_INCLUDE_ASM define is checked by our shared C/C++/assembler headers,
 * and used to provide declarations compatible with the preprocessor and assembler. */
#define PLCRASH_UNWIND_TEST_ASM 1
#include "UnwindTestType.hpp"

/*
 * Define platform-width pointer constant
 *
 * $0 - The pointer's value
 */
.macro unwind_ptr
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
    .long $0
#elif defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 8
    .quad $0
#else
#    error __SIZEOF_POINTER__ is undefined, or defined to an unexpected value. Please define an appropriate macro for this toolchain.
#endif
.endmacro

/*
 * Define an UnwindTest test function's name and global symbol. This should be coupled
 * with a trailing call to UnwindTestFunctionEnd
 *
 * $0 - The test's symbol name, minus Darwin's leading '_' (eg, 'unwind_test_frame_ebx').
 *
 * Example:
 * UnwindTestFunction unwind_test_frame_ebx, UNWIND_TEST_TYPE_DEFAULT
 */
.macro UnwindTestFunction
    /* Add a name constant */
    .section __DATA,__const
    .globl _$0_name
    _$0_name:
        .asciz "$0"

    /* Now declare the actual test implementation function. */
    .text

    /* Ensure proper alignment for the given architecture */
#if __i386__ || __x86_64__
#elif __arm__ || __arm64__
    .align 2
#else
#error Define an appropriate alignment for this platform
#endif

    /* Declare our test function's symbol */
    .globl _$0
    _$0:
.endmacro

/*
 * Terminate a test function previously declared via UnwindTestFunction
 *
 * $0 - The test's symbol name, minus Darwin's leading '_' (eg, 'unwind_test_frame_ebx').
 */
.macro UnwindTestFunctionEnd
    /* Add a symbol for the test function's end. */
    .globl _$0_end
    _$0_end:
.endmacro

#endif /* PLCRASH_UNWIND_TEST_S */
