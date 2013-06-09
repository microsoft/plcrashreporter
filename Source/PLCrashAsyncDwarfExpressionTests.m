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


/*
 * Configure the test cases for thread states that are supported by the host.
 *
 * The primary test validates the 64-bit variation (on hosts that support it). The
 * _32 test case subclass variant validates 32-bit operation
 */
#ifdef PLCRASH_ASYNC_THREAD_X86_SUPPORT
#    define TEST_THREAD_64_CPU CPU_TYPE_X86_64
#    define TEST_THREAD_64_DWARF_REG1 14 // r14
#    define TEST_THREAD_64_DWARF_REG_INVALID 99 // unhandled DWARF register number

#    define TEST_THREAD_32_CPU CPU_TYPE_X86
#    define TEST_THREAD_32_DWARF_REG1 8 // EIP
#    define TEST_THREAD_32_DWARF_REG_INVALID 99 // unhandled DWARF register number

#elif PLCRASH_ASYNC_THREAD_ARM_SUPPORT

#    define TEST_THREAD_64_CPU CPU_TYPE_ARM
#    define TEST_THREAD_64_DWARF_REG1 14 // LR (r14)
#    define TEST_THREAD_64_DWARF_REG_INVALID 99 // unhandled DWARF register number

#    define TEST_THREAD_32_CPU CPU_TYPE_ARM
#    define TEST_THREAD_32_DWARF_REG1 14 // LR (r14)
#    define TEST_THREAD_32_DWARF_REG_INVALID 99 // unhandled DWARF register number

#else
#    error Add support for this platform
#endif


@interface PLCrashAsyncDwarfExpressionTests : PLCrashTestCase {
@protected
    plcrash_async_thread_state_t _ts;
}

- (cpu_type_t) targetCPU;
- (uint64_t) dwarfTestRegister;
- (uint64_t) dwarfBadRegister;

@end

@interface PLCrashAsyncDwarfExpressionTests_32 : PLCrashAsyncDwarfExpressionTests @end
@implementation PLCrashAsyncDwarfExpressionTests_32 @end

/**
 * Test DWARF expression evaluation.
 */
@implementation PLCrashAsyncDwarfExpressionTests

- (void) setUp {
    STAssertEquals(PLCRASH_ESUCCESS, plcrash_async_thread_state_init(&_ts, [self targetCPU]), @"Failed to initialize thread state");
}

/* Return the 32/64 configuration values */

- (BOOL) is32 {
    return [[self class] isEqual: [PLCrashAsyncDwarfExpressionTests_32 class]];
}

- (cpu_type_t) targetCPU {
    if ([self is32])
        return TEST_THREAD_32_CPU;
    else
        return TEST_THREAD_64_CPU;
}

- (uint64_t) dwarfTestRegister {
    if ([self is32])
        return TEST_THREAD_32_DWARF_REG1;
    else
        return TEST_THREAD_64_DWARF_REG1;
}

- (uint64_t) dwarfBadRegister {
    if ([self is32])
        return TEST_THREAD_32_DWARF_REG_INVALID;
    else
        return TEST_THREAD_64_DWARF_REG_INVALID;
}

/* Perform evaluation of the given opcodes, expecting a result of type @a type,
 * with an expected value of @a expected. The data is interpreted as big endian,
 * as to simplify formulating multi-byte test values in the opcode stream */
#define PERFORM_EVAL_TEST(opcodes, type, expected) do { \
    plcrash_async_mobject_t mobj; \
    plcrash_error_t err; \
    STAssertEquals(PLCRASH_ESUCCESS, plcrash_async_mobject_init(&mobj, mach_task_self(), &opcodes, sizeof(opcodes), true), @"Failed to initialize mobj"); \
\
    if ([self targetCPU] & CPU_ARCH_ABI64) { \
        uint64_t result; \
        err = plcrash_async_dwarf_eval_expression_64(&mobj, &_ts, plcrash_async_byteorder_big_endian(), &opcodes, 0, sizeof(opcodes), &result); \
        STAssertEquals(err, PLCRASH_ESUCCESS, @"64-bit evaluation failed"); \
        STAssertEquals((type)result, (type)expected, @"Incorrect 64-bit result"); \
    } else { \
        uint32_t result; \
        err = plcrash_async_dwarf_eval_expression_32(&mobj, &_ts, plcrash_async_byteorder_big_endian(), &opcodes, 0, sizeof(opcodes), &result); \
        STAssertEquals(err, PLCRASH_ESUCCESS, @"32-bit evaluation failed"); \
        STAssertEquals((type)result, (type)expected, @"Incorrect 32-bit result"); \
    } \
\
    plcrash_async_mobject_free(&mobj); \
} while(0)



/**
 * Test evaluation of the DW_OP_litN opcodes.
 */
- (void) testLitN {
    for (uint64_t i = 0; i < (DW_OP_lit31 - DW_OP_lit0); i++) {
        uint8_t opcodes[] = {
            DW_OP_lit0 + i // The opcodes are defined in monotonically increasing order.
        };
        
        PERFORM_EVAL_TEST(opcodes, uint64_t, i);
    }
}

/**
 * Test evaluation of the DW_OP_const1u opcode
 */
- (void) testConst1u {
    uint8_t opcodes[] = { DW_OP_const1u, 0xFF };
    PERFORM_EVAL_TEST(opcodes, uint8_t, 0xFF);
}

/**
 * Test evaluation of the DW_OP_const1s opcode
 */
- (void) testConst1s {
    uint8_t opcodes[] = { DW_OP_const1s, 0x80 };
    PERFORM_EVAL_TEST(opcodes, int8_t, INT8_MIN);
}

/**
 * Test evaluation of the DW_OP_const2u opcode
 */
- (void) testConst2u {
    uint8_t opcodes[] = { DW_OP_const2u, 0xFF, 0xFA};
    PERFORM_EVAL_TEST(opcodes, uint16_t, 0xFFFA);
}

/**
 * Test evaluation of the DW_OP_const2s opcode
 */
- (void) testConst2s {
    uint8_t opcodes[] = { DW_OP_const2s, 0x80, 0x00 };
    PERFORM_EVAL_TEST(opcodes, int16_t, INT16_MIN);
}

/**
 * Test evaluation of the DW_OP_const4u opcode
 */
- (void) testConst4u {
    uint8_t opcodes[] = { DW_OP_const4u, 0xFF, 0xFF, 0xFF, 0xFA};
    PERFORM_EVAL_TEST(opcodes, uint32_t, 0xFFFFFFFA);
}

/**
 * Test evaluation of the DW_OP_const4s opcode
 */
- (void) testConst4s {
    uint8_t opcodes[] = { DW_OP_const4s, 0x80, 0x00, 0x00, 0x00 };
    PERFORM_EVAL_TEST(opcodes, int32_t, INT32_MIN);
}

/**
 * Test evaluation of the DW_OP_const8u opcode
 */
- (void) testConst8u {
    uint8_t opcodes[] = { DW_OP_const8u, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFA};

    /* Test for the expected overflow for 32-bit targets */
    if ([self is32])
        PERFORM_EVAL_TEST(opcodes, uint32_t, (uint32_t)0xFFFFFFFFFFFFFFFA);
    else
        PERFORM_EVAL_TEST(opcodes, uint64_t, 0xFFFFFFFFFFFFFFFA);
}

/**
 * Test evaluation of the DW_OP_const8s opcode
 */
- (void) testConst8s {
    uint8_t opcodes[] = { DW_OP_const8s, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    
    /*
     * Signed overflow behavior is left to the implementation by the C99 standard, and the
     * implementation is explicitly permitted to raise a signal. Since we are executing
     * potentially invalid/corrupt/untrusted bytecode, we need to be sure that evaluation
     * does not trigger this behavior.
     *
     * Our implementation will always cast signed types to unsigned types during truncation,
     * which should exhibit defined (if not particularly useful) behavior. The 32-bit
     * variation of this test serves as a rather loose smoke test for that handling, rather
     * than demonstrating any useful properties of the truncation
     */
    if ([self is32])
        PERFORM_EVAL_TEST(opcodes, int64_t , (uint32_t)INT64_MIN);
    else
        PERFORM_EVAL_TEST(opcodes, int64_t , INT64_MIN);
}


/**
 * Test evaluation of the DW_OP_constu (ULEB128 constant) opcode
 */
- (void) testConstu {
    uint8_t opcodes[] = { DW_OP_constu, 0+0x80, 0x1 };
    PERFORM_EVAL_TEST(opcodes, uint8_t, 128);
}

/**
 * Test evaluation of the DW_OP_consts (SLEB128 constant) opcode
 */
- (void) testConsts {
    uint8_t opcodes[] = { DW_OP_consts, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x7f};
    
    /*
     * Signed overflow behavior is left to the implementation by the C99 standard, and the
     * implementation is explicitly permitted to raise a signal. Since we are executing
     * potentially invalid/corrupt/untrusted bytecode, we need to be sure that evaluation
     * does not trigger this undefined behavior.
     *
     * Our implementation will always cast signed types to unsigned types during truncation,
     * which should exhibit defined (if not particularly useful) behavior. The 32-bit
     * variation of this test serves as a rather loose smoke test for that handling, rather
     * than demonstrating any useful properties of the truncation
     */
    if ([self is32])
        PERFORM_EVAL_TEST(opcodes, uint32_t, (uint32_t)INT64_MIN);
    else
        PERFORM_EVAL_TEST(opcodes, int64_t, INT64_MIN);
}

/**
 * Test evaluation of DW_OP_bregN opcodes.
 */
- (void) testBreg {
    /* Set up the thread state */
    plcrash_async_thread_state_set_reg(&_ts, plcrash_async_thread_state_map_dwarf_reg(&_ts, [self dwarfTestRegister]), 0xFF);

    /* Should evaluate to value of the TEST_THREAD_DWARF_REG1 register, plus 5 (the value is sleb128 encoded) */
    uint8_t opcodes[] = { DW_OP_breg0 + [self dwarfTestRegister], 0x5 };
    PERFORM_EVAL_TEST(opcodes, uint64_t, 0xFF+5);
    
    /* Should evaluate to value of the TEST_THREAD_DWARF_REG1 register, minus 2 (the value is sleb128 encoded)*/
    uint8_t opcodes_negative[] = { DW_OP_breg0 + [self dwarfTestRegister], 0x7e };
    PERFORM_EVAL_TEST(opcodes_negative, uint64_t, 0xFF-2);
}

/** Test basic evaluation of a NOP. */
- (void) testNop {
    uint8_t opcodes[] = {
        DW_OP_nop,
        DW_OP_lit31 // at least one result must be available
    };
    
    PERFORM_EVAL_TEST(opcodes, uint64_t, 31);
}

/**
 * Test handling of an empty result.
 */
- (void) testEmptyStackResult {
    plcrash_async_thread_state_t ts;
    plcrash_async_mobject_t mobj;
    plcrash_error_t err;
    uint32_t result;
    uint8_t opcodes[] = {
        DW_OP_nop // push nothing onto the stack
    };

    STAssertEquals(PLCRASH_ESUCCESS, plcrash_async_thread_state_init(&ts, [self targetCPU] & ~CPU_ARCH_ABI64), @"Failed to initialize thread state");
    STAssertEquals(PLCRASH_ESUCCESS, plcrash_async_mobject_init(&mobj, mach_task_self(), &opcodes, sizeof(opcodes), true), @"Failed to initialize mobj");
    
    err = plcrash_async_dwarf_eval_expression_32(&mobj, &ts, &plcrash_async_byteorder_direct, &opcodes, 0, sizeof(opcodes), &result);
    STAssertEquals(err, PLCRASH_EINVAL, @"Evaluation of a no-result expression should have failed with EINVAL");

    plcrash_async_mobject_free(&mobj);
}

/**
 * Test invalid opcode handling
 */
- (void) testInvalidOpcode {
    plcrash_async_thread_state_t ts;
    plcrash_async_mobject_t mobj;
    plcrash_error_t err;
    uint32_t result;
    uint8_t opcodes[] = {
        // Arbitrarily selected bad instruction value.
        // This -could- be allocated to an opcode in the future, but
        // then our test will fail and we can pick another one.
        0x0 
    };
    
    STAssertEquals(PLCRASH_ESUCCESS, plcrash_async_thread_state_init(&ts, [self targetCPU] & ~CPU_ARCH_ABI64), @"Failed to initialize thread state");
    STAssertEquals(PLCRASH_ESUCCESS, plcrash_async_mobject_init(&mobj, mach_task_self(), &opcodes, sizeof(opcodes), true), @"Failed to initialize mobj");
    
    err = plcrash_async_dwarf_eval_expression_32(&mobj, &ts, &plcrash_async_byteorder_direct, &opcodes, 0, sizeof(opcodes), &result);
    STAssertEquals(err, PLCRASH_ENOTSUP, @"Evaluation of a bad opcode should have failed with ENOTSUP");
    
    plcrash_async_mobject_free(&mobj);
}


@end