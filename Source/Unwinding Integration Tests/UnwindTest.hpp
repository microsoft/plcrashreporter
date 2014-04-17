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

#ifndef PLCRASH_UNWIND_TEST_H
#define PLCRASH_UNWIND_TEST_H 1

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include "PLCrashMacros.h"
#include "UnwindTestType.hpp"

namespace plcrash {

/**
 * An unwind test function implementation. The function is responsible for:
 *
 * - Setting up any test data necessary, including saving and overwriting volatile registers,
 *   adjusting the stack pointer, overwriting the stack pointer, etc.
 * - Dispatching a call to unwind_test_validate()
 * - Cleaning up and returning to the caller.
 *
 * The test function implementor is responsible for providing the unwind data necessary to unwind past
 * the test function. The unwind mechanisms supported by a test function are defined as part of the
 * UnwindTest class.
 */
typedef void (unwind_test_function_t)(void);

/**
 * An unwind test.
 *
 * @warning The structure layout and semantics must be kept in sync with the assembler macros
 * declared in `UnwindTest.S`
 */
typedef struct UnwindTest {
private:
    /** NUL terminated test name. */
    const char *_name;

    /** The test function to be called. */
    unwind_test_function_t * const _test_function;

    /** The unwinder types this test exercises. */
    const UnwindTestType _types;

public:
    UnwindTest (const char *name, unwind_test_function_t *impl, UnwindTestType types);
    
    static UnwindTest TERM ();

    bool invoke (void) const;
    
    // getters
    const char *name (void) const;
    bool isTerminator (void) const;
    UnwindTestType types (void) const;
} UnwindTest;

} /* namespace plcrash */

#endif /* PLCRASH_UNWIND_TEST_H */
