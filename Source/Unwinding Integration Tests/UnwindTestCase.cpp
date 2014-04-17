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

#include "UnwindTestCase.hpp"
#include "PLCrashAsync.h"

using namespace plcrash;

UnwindTest tests[] = {
    UnwindTest("Foo", NULL, UnwindTestTypeCompactUnwind),
    UnwindTest::TERM()
};

UnwindTestCase tc = {
    UnwindTestCase("tc!", tests)
};

/**
 * Construct a new test case instance.
 *
 * @param name Test case name. This string will not be copied; it must remain valid for the lifetime of the test instance.
 * @param types Unwinding implementations supported by this test.
 */
UnwindTestCase::UnwindTestCase (const char *name, const UnwindTest tests[]) : _name(name), _tests(tests) {}

/**
 * Execute all the tests in this test case.
 *
 * Returns true if all tests succeed. Terminates on the first failed test.
 */
bool UnwindTestCase::executeAll (void) const {
    for (size_t i = 0; _tests[i].isTerminator() == false; i++) {
        PLCF_DEBUG("Running %s", _tests[i].name());
        
        if (_tests[i].invoke()) {
            return false;
        }
    }
    
    return true;
}
