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

#include "UnwindTest.hpp"
#include <string.h>

using namespace plcrash;

/**
 * Target-specific test invoker. Responsible for initializing non-volatile registers and
 * ensuring that the they're left unmodified upon return from @a test.
 *
 * @param test The test function to invoke.
 * @param sp Will be populated with the expected restored value of the stack pointer.
 *
 * @return True on if all non-volatile registers were successfully restored in unwinding.
 */
PLCR_C_EXPORT bool unwind_test_invoke (unwind_test_function_t *test, void **sp);

/**
 * Return an UnwindTest instance that may be used as an UnwindTest array terminator.
 */
UnwindTest UnwindTest::TERM () {
    return UnwindTest(NULL, NULL, UnwindTestTypeDefault);
}

/**
 * Construct a new test instance.
 *
 * @param name Name. This string will not be copied; it must remain valid for the lifetime of the test instance.
 * @param impl Test implementation function.
 * @param types Unwinding implementations supported by this test.
 */
UnwindTest::UnwindTest (const char *name, unwind_test_function_t *impl, UnwindTestType types) : _name(name), _test_function(impl), _types(types) {}

/**
 * Return a borrowed reference to the test's (NUL terminated) name.
 */
const char *UnwindTest::name (void) const {
    return _name;
}

/**
 * Invoke the test, returning true on success.
 */
bool UnwindTest::invoke (void) const {
    /* TODO: We don't use SP */
    void *sp;
    return unwind_test_invoke(_test_function, &sp);
}

/**
 * Returns true if this is a NULL terminating entry.
 */
bool UnwindTest::isTerminator (void) const {
    return _test_function == NULL;
}

/**
 * Return the test's defined flags.
 */
UnwindTestType UnwindTest::types (void) const {
    return _types;
}
