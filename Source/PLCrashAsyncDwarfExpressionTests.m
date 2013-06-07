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

#include "PLCrashAsyncDwarfExpression.h"

@interface PLCrashAsyncDwarfExpressionTests : PLCrashTestCase {
}
@end

/**
 * Test DWARF expression evaluation.
 */
@implementation PLCrashAsyncDwarfExpressionTests

/**
 * Test evaluation of the DW_OP_litN opcodes.
 */
- (void) testLitN {
    plcrash_async_mobject_t mobj;
    plcrash_error_t err;
    uint64_t result;
    
    for (uint64_t i = 0; i < (DW_OP_lit31 - DW_OP_lit0); i++) {
        uint8_t opcodes[] = {
            DW_OP_lit0 + i // The opcodes are defined in order.
        };
    
        STAssertEquals(PLCRASH_ESUCCESS, plcrash_async_mobject_init(&mobj, mach_task_self(), &opcodes, sizeof(opcodes), true), @"Failed to initialize mobj");
        
        err = plcrash_async_dwarf_eval_expression(&mobj, 8, &plcrash_async_byteorder_direct, &opcodes, 0, sizeof(opcodes), &result);
        STAssertEquals(err, PLCRASH_ESUCCESS, @"Evaluation failed");
        STAssertEquals(result, (uint64_t) i, @"Incorrect result");
    }
}

/**
 * Test basic evaluation of a NOP.
 */
- (void) testNop {
    plcrash_async_mobject_t mobj;
    plcrash_error_t err;
    uint64_t result;
    uint8_t opcodes[] = {
        DW_OP_nop,
        DW_OP_lit0 // at least one result must be available
    };

    STAssertEquals(PLCRASH_ESUCCESS, plcrash_async_mobject_init(&mobj, mach_task_self(), &opcodes, sizeof(opcodes), true), @"Failed to initialize mobj");

    err = plcrash_async_dwarf_eval_expression(&mobj, 8, &plcrash_async_byteorder_direct, &opcodes, 0, sizeof(opcodes), &result);
    STAssertEquals(err, PLCRASH_ESUCCESS, @"Evaluation failed");
    STAssertEquals(result, (uint64_t) 0, @"Incorrect result");
}

/**
 * Test handling of an empty result.
 */
- (void) testEmptyStackResult {
    plcrash_async_mobject_t mobj;
    plcrash_error_t err;
    uint64_t result;
    uint8_t opcodes[] = {
        DW_OP_nop // push nothing onto the stack
    };
    
    STAssertEquals(PLCRASH_ESUCCESS, plcrash_async_mobject_init(&mobj, mach_task_self(), &opcodes, sizeof(opcodes), true), @"Failed to initialize mobj");
    
    err = plcrash_async_dwarf_eval_expression(&mobj, 8, &plcrash_async_byteorder_direct, &opcodes, 0, sizeof(opcodes), &result);
    STAssertEquals(err, PLCRASH_EINVAL, @"Evaluation of a no-result expression should have failed with EINVAL");
}

/**
 * Test invalid opcode handling
 */
- (void) testInvalidOpcode {
    plcrash_async_mobject_t mobj;
    plcrash_error_t err;
    uint64_t result;
    uint8_t opcodes[] = {
        // Arbitrarily selected bad instruction value.
        // This -could- be allocated to an opcode in the future, but
        // then our test will fail and we can pick another one.
        0x0 
    };
    
    STAssertEquals(PLCRASH_ESUCCESS, plcrash_async_mobject_init(&mobj, mach_task_self(), &opcodes, sizeof(opcodes), true), @"Failed to initialize mobj");
    
    err = plcrash_async_dwarf_eval_expression(&mobj, 8, &plcrash_async_byteorder_direct, &opcodes, 0, sizeof(opcodes), &result);
    STAssertEquals(err, PLCRASH_ENOTSUP, @"Evaluation of a bad opcode should have failed with ENOTSUP");
}


@end