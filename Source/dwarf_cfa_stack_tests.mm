/*
 * Copyright (c) 2013 Plausible Labs Cooperative, Inc.
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

#include "dwarf_cfa_stack.h"

using namespace plcrash;

@interface dwarf_cfa_stack_tests : PLCrashTestCase {
@private
}
@end

/**
 * Test DWARF CFA stack implementation.
 */
@implementation dwarf_cfa_stack_tests

- (void) testSetRegister {
    dwarf_cfa_stack<uint32_t, 100> stack;

    /* Try using all available entries */
    for (int i = 0; i < 100; i++) {
        STAssertTrue(stack.set_register(i, DWARF_CFA_REG_RULE_OFFSET, i), @"Failed to add register");
        STAssertEquals((uint8_t)(i+1), stack.get_register_count(), @"Incorrect number of registers");
    }

    /* Ensure that additional requests fail */
    STAssertFalse(stack.set_register(100, DWARF_CFA_REG_RULE_OFFSET, 100), @"A register was somehow allocated from an empty free list");
    
    /* Verify that modifying an already-added register succeeds */
    STAssertTrue(stack.set_register(0, DWARF_CFA_REG_RULE_OFFSET, 0), @"Failed to modify existing register");
    STAssertEquals((uint8_t)100, stack.get_register_count(), @"Register count was bumped when modifying an existing register");

    /* Verify the register values that were added */
    for (uint32_t i = 0; i < 100; i++) {
        dwarf_cfa_reg_rule_t rule;
        uint32_t value;
        
        STAssertTrue(stack.get_register_rule(i, &rule, &value), @"Failed to fetch info for entry");
        STAssertEquals(i, value, @"Incorrect value");
        STAssertEquals(rule, DWARF_CFA_REG_RULE_OFFSET, @"Incorrect rule");
    }
}

- (void) testEnumerateRegister {
    
}

- (void) testRemoveRegister {
    
}

@end