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

#include "dwarf_stack.h"

using namespace plcrash;

@interface dwarf_stack_tests : PLCrashTestCase {
    
}
@end

/**
 * Test DWARF stack handling.
 */
@implementation dwarf_stack_tests

/**
 * Test basic push/pop handling.
 */
- (void) testPushPop {
    dwarf_stack<int, 2> stack;
    int v;

    STAssertTrue(stack.push(1), @"push failed");
    STAssertTrue(stack.push(2), @"push failed");

    STAssertTrue(stack.pop(&v), @"pop failed");
    STAssertEquals(2, v, @"Incorrect value popped");
    
    STAssertTrue(stack.pop(&v), @"pop failed");
    STAssertEquals(1, v, @"Incorrect value popped");
}

/**
 * Test a pop on an empty stack.
 */
- (void) testPopEmpty {
    dwarf_stack<int, 1> stack;
    int v;
    STAssertFalse(stack.pop(&v), @"pop should have failed on empty stack");
}

/**
 * Test a push on a full stack.
 */
- (void) testPushFull {
    dwarf_stack<int, 2> stack;
    STAssertTrue(stack.push(1), @"push failed");
    STAssertTrue(stack.push(2), @"push failed");
    STAssertFalse(stack.push(3), @"push succeeded on a full stack");
}

@end