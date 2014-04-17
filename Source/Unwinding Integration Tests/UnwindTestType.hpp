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

/* NOTE: This file is included by pre-processed assembler files; prior to including
 * this header, PLCRASH_UNWIND_TEST_ASM must be defined. */

#ifndef PLCRASH_UNWIND_TEST_TYPE_H
#define PLCRASH_UNWIND_TEST_TYPE_H 1

/* Assembler-visible constants */
#define UNWIND_TEST_TYPE_COMPACT_UNWIND (1 << 1)
#define UNWIND_TEST_TYPE_DWARF          (1 << 2)
#define UNWIND_TEST_TYPE_FRAME          (1 << 3)
#define UNWIND_TEST_TYPE_DEFAULT        (UNWIND_TEST_TYPE_COMPACT_UNWIND | UNWIND_TEST_TYPE_DWARF | UNWIND_TEST_TYPE_FRAME)

/* C++ equivalent constants */
#ifndef PLCRASH_UNWIND_TEST_ASM
/**
 * Supported test type flags. Each flag specifies an unwinding implementation that should be able to unwind
 * a given test.
 */
typedef enum : uint32_t {
    /**
     * The test function provides compact unwind and dwarf unwinding metadata, and
     * additionally maintains a frame pointer to support frame-based unwinding.
     */
    UnwindTestTypeCompactUnwind     = UNWIND_TEST_TYPE_COMPACT_UNWIND,
    
    /** The test function provdes a valid compact unwind entry. */
    UnwindTestTypeDwarf             = UNWIND_TEST_TYPE_DWARF,
    
    /** The test function provides a valid DWARF entry. */
    UnwindTestTypeFrame             = UNWIND_TEST_TYPE_FRAME,
    
    /**
     * The test function provides full unwinding metadata for all supported unwinding mechanisms, and
     * additionally maintains a frame pointer to support frame-based unwinding.
     */
    UnwindTestTypeDefault           = UNWIND_TEST_TYPE_DEFAULT
} UnwindTestType;
#endif /* !PLCRASH_UNWIND_TEST_ASM */

#endif /* PLCRASH_UNWIND_TEST_TYPE_H */
