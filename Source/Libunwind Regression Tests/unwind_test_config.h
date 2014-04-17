/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
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

/* NOTE: This file is shared between C and pre-processed assembly files */

#ifndef UNWIND_TEST_CONFIG_H
#define UNWIND_TEST_CONFIG_H 1

/** The test function does not provide a valid compact unwind entry. */
#define UNWIND_TEST_NO_COMPACT_UNWIND  (1 << 1)

/** The test function does not provide a valid DWARF entry. */
#define UNWIND_TEST_NO_DWARF           (1 << 2)

/** The test function does not maintain a valid frame pointer. */
#define UNWIND_TEST_NO_FRAME           (1 << 3)

/* Assembler directives that should be excluded when included from a non-assembler file */
#if !UNWIND_TEST_CONFIG_NON_ASSEMBLER_IMPORT

/**
 * Decare an unwind_test_entry_t.
 *
 * $0 - The test function entry point.
 * $1 - The unwind_test_flags_t value for this test.
 */
#if __i386__ || __arm__
.macro unwind_declare_test
.long $0
.long $1
.endmacro
#elif __x86_64__ || __arm64__
.macro unwind_declare_test
.quad $0
.long $1
.space 4
.endmacro
#else
#error Select (or add) an appropriate declaration macro for this platform
#endif

/**
 * Declare a NULL (terminating) unwind_test_entry_t.
 */
.macro unwind_declare_test_null
unwind_declare_test 0, 0
.endmacro


#endif /* UNWIND_TEST_CONFIG_NON_ASSEMBLER_IMPORT */


#endif /* UNWIND_TEST_CONFIG_H */
