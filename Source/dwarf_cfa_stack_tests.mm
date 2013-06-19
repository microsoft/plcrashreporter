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

#include "dwarf_cfa_stack.hpp"
#include <inttypes.h>

using namespace plcrash;

@interface dwarf_cfa_stack_tests : PLCrashTestCase {
@private
}
@end

/**
 * Test DWARF CFA stack implementation.
 */
@implementation dwarf_cfa_stack_tests

/**
 * Test setting registers in the current state.
 */
- (void) testSetRegister {
    dwarf_cfa_stack<uint32_t, 100> stack;

    /* Try using all available entries */
    for (int i = 0; i < 100; i++) {
        STAssertTrue(stack.set_register(i, PLCRASH_DWARF_CFA_REG_RULE_OFFSET, i), @"Failed to add register");
        STAssertEquals((uint8_t)(i+1), stack.get_register_count(), @"Incorrect number of registers");
    }

    /* Ensure that additional requests fail */
    STAssertFalse(stack.set_register(100, PLCRASH_DWARF_CFA_REG_RULE_OFFSET, 100), @"A register was somehow allocated from an empty free list");
    
    /* Verify that modifying an already-added register succeeds */
    STAssertTrue(stack.set_register(0, PLCRASH_DWARF_CFA_REG_RULE_OFFSET, 0), @"Failed to modify existing register");
    STAssertEquals((uint8_t)100, stack.get_register_count(), @"Register count was bumped when modifying an existing register");

    /* Verify the register values that were added */
    for (uint32_t i = 0; i < 100; i++) {
        plcrash_dwarf_cfa_reg_rule_t rule;
        uint32_t value;
        
        STAssertTrue(stack.get_register_rule(i, &rule, &value), @"Failed to fetch info for entry");
        STAssertEquals(i, value, @"Incorrect value");
        STAssertEquals(rule, PLCRASH_DWARF_CFA_REG_RULE_OFFSET, @"Incorrect rule");
    }
}

/**
 * Test enumerating registers in the current state.
 */
- (void) testEnumerateRegisters {
    dwarf_cfa_stack<uint32_t, 32> stack;

    /* Allocate all available entries */
    for (int i = 0; i < 32; i++) {
        STAssertTrue(stack.set_register(i, PLCRASH_DWARF_CFA_REG_RULE_OFFSET, i), @"Failed to add register");
        STAssertEquals((uint8_t)(i+1), stack.get_register_count(), @"Incorrect number of registers");
    }
    
    /* Enumerate */
    dwarf_cfa_stack_iterator<uint32_t, 32> iter = dwarf_cfa_stack_iterator<uint32_t, 32>(&stack);
    uint32_t found_set = UINT32_MAX;
    uint32_t regnum;
    plcrash_dwarf_cfa_reg_rule_t rule;
    uint32_t value;

    for (int i = 0; i < 32; i++) {
        STAssertTrue(iter.next(&regnum, &rule, &value), @"Iteration failed while additional registers remain");
        STAssertEquals(regnum, value, @"Unexpected value");
        STAssertEquals(rule, PLCRASH_DWARF_CFA_REG_RULE_OFFSET, @"Incorrect rule");
        
        found_set &= ~(1<<i);
    }
    
    STAssertFalse(iter.next(&regnum, &rule, &value), @"Iteration succeeded after successfully iterating all registers (got regnum=%" PRIu32 ")", regnum);
    
    STAssertEquals(found_set, (uint32_t)0, @"Did not enumerate all 32 values: 0x%" PRIx32, found_set);
}

/**
 * Test removing register values from the current state.
 */
- (void) testRemoveRegister {
    dwarf_cfa_stack<uint32_t, 100> stack;
    
    /* Insert rules for all entries */
    for (int i = 0; i < 100; i++) {
        STAssertTrue(stack.set_register(i, PLCRASH_DWARF_CFA_REG_RULE_OFFSET, i), @"Failed to add register");
        STAssertEquals((uint8_t)(i+1), stack.get_register_count(), @"Incorrect number of registers");
    }

    /* Remove a quarter of the entries */
    uint8_t remove_count = 0;
    for (int i = 0; i < 100; i++) {
        if (i % 2) {
            stack.remove_register(i);
            remove_count++;
        }
    }

    STAssertEquals(stack.get_register_count(), (uint8_t)(100-remove_count), @"Register count was not correctly updated");
    
    /* Verify the full set of registers (including verifying that the removed registers were, in fact, removed) */
    for (uint32_t i = 0; i < 100; i++) {
        plcrash_dwarf_cfa_reg_rule_t rule;
        uint32_t value;
        
        if (i % 2) {
            STAssertFalse(stack.get_register_rule(i, &rule, &value), @"Register info was returned for a removed register");
        } else {
            STAssertTrue(stack.get_register_rule(i, &rule, &value), @"Failed to fetch info for entry");
            STAssertEquals(i, value, @"Incorrect value");
            STAssertEquals(rule, PLCRASH_DWARF_CFA_REG_RULE_OFFSET, @"Incorrect rule");
        }
    }
    
    /* Re-add the missing registers (verifying that they were correctly added to the free list) */
    for (int i = 0; i < 100; i++) {
        if (i % 2)
            STAssertTrue(stack.set_register(i, PLCRASH_DWARF_CFA_REG_RULE_OFFSET, i), @"Failed to add register");
    }
    
    STAssertEquals(stack.get_register_count(), (uint8_t)100, @"Register count was not correctly updated");
    
    /* Ensure that additional requests fail */
    STAssertFalse(stack.set_register(101, PLCRASH_DWARF_CFA_REG_RULE_OFFSET, 101), @"A register was somehow allocated from an empty free list");
    
    /* Verify the register values that were added */
    for (uint32_t i = 0; i < 100; i++) {
        plcrash_dwarf_cfa_reg_rule_t rule;
        uint32_t value;
        
        STAssertTrue(stack.get_register_rule(i, &rule, &value), @"Failed to fetch info for entry");
        STAssertEquals(i, value, @"Incorrect value");
        STAssertEquals(rule, PLCRASH_DWARF_CFA_REG_RULE_OFFSET, @"Incorrect rule");
    }
}

/**
 * Test pushing and popping of register state.
 */
- (void) testPushPopState {
    dwarf_cfa_stack<uint32_t, 100> stack;

    /* Verify that popping an empty stack returns an error */
    STAssertFalse(stack.pop_state(), @"Popping succeeded on an empty state stack");
    
    /* Configure initial test state */
    for (int i = 0; i < 25; i++) {
        STAssertTrue(stack.set_register(i, PLCRASH_DWARF_CFA_REG_RULE_OFFSET, i), @"Failed to add register");
        STAssertEquals((uint8_t)(i+1), stack.get_register_count(), @"Incorrect number of registers");
    }
    
    /* Try pushing and initializing new state */
    STAssertTrue(stack.push_state(), @"Failed to push a new state");
    STAssertEquals((uint8_t)0, stack.get_register_count(), @"New state should have a register count of 0");

    for (int i = 25; i < 50; i++) {
        STAssertTrue(stack.set_register(i, PLCRASH_DWARF_CFA_REG_RULE_OFFSET, i), @"Failed to add register");
        STAssertEquals((uint8_t)(i-24), stack.get_register_count(), @"Incorrect number of registers");
    }
    
    /* Pop the state, verify that our original registers were saved */
    STAssertTrue(stack.pop_state(), @"Failed to pop current state");
    for (uint32_t i = 0; i < 25; i++) {
        plcrash_dwarf_cfa_reg_rule_t rule;
        uint32_t value;
        
        STAssertTrue(stack.get_register_rule(i, &rule, &value), @"Failed to fetch info for entry");
        STAssertEquals(i, value, @"Incorrect value");
        STAssertEquals(rule, PLCRASH_DWARF_CFA_REG_RULE_OFFSET, @"Incorrect rule");
    }
    
    /* Validate state overflow checking; an implicit state already exists at the top of the stack, so we
     * start iteration at 1. */
    for (uint8_t i = 1; i < DWARF_CFA_STACK_MAX_STATES; i++) {
        STAssertTrue(stack.push_state(), @"Failed to push a new state");
    }
    STAssertFalse(stack.push_state(), @"Pushing succeeded on a full state stack");
}

@end