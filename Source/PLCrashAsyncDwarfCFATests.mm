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

#include "PLCrashAsyncDwarfCFA.hpp"
#include "PLCrashAsyncDwarfExpression.h"

#define DW_CFA_BAD_OPCODE DW_CFA_hi_user

@interface PLCrashAsyncDwarfCFATests : PLCrashTestCase {
    plcrash::dwarf_cfa_state _stack;
    plcrash_async_dwarf_gnueh_ptr_state_t _ptr_state;
    plcrash_async_dwarf_cie_info_t _cie;
}
@end

/**
 * Test DWARF CFA interpretation.
 */
@implementation PLCrashAsyncDwarfCFATests

- (void) setUp {
    /* Initialize required configuration for pointer dereferencing */
    plcrash_async_dwarf_gnueh_ptr_state_init(&_ptr_state, 4);

    _cie.segment_size = 0x0; // we don't use segments
    _cie.has_eh_augmentation = true;
    _cie.eh_augmentation.has_pointer_encoding = true;
    _cie.eh_augmentation.pointer_encoding = DW_EH_PE_absptr; // direct pointers
    
    _cie.code_alignment_factor = 1;
    _cie.data_alignment_factor = 1;
    
    _cie.address_size = _ptr_state.address_size;
}

- (void) tearDown {
    plcrash_async_dwarf_gnueh_ptr_state_free(&_ptr_state);
}

/* Perform evaluation of the given opcodes, expecting a result of type @a type,
 * with an expected value of @a expected. The data is interpreted as big endian. */
#define PERFORM_EVAL_TEST(opcodes, pc_offset, expected) do { \
    plcrash_async_mobject_t mobj; \
    plcrash_error_t err; \
    STAssertEquals(PLCRASH_ESUCCESS, plcrash_async_mobject_init(&mobj, mach_task_self(), (pl_vm_address_t) &opcodes, sizeof(opcodes), true), @"Failed to initialize mobj"); \
    \
        err = plcrash_async_dwarf_eval_cfa_program(&mobj, (pl_vm_address_t)pc_offset, &_cie, &_ptr_state, plcrash_async_byteorder_big_endian(), (pl_vm_address_t) &opcodes, 0, sizeof(opcodes), &_stack); \
        STAssertEquals(err, expected, @"Evaluation failed"); \
    \
    plcrash_async_mobject_free(&mobj); \
} while(0)

/** Test evaluation of DW_CFA_set_loc */
- (void) testSetLoc {
    /* This should terminate once our PC offset is hit below; otherwise, it will execute a
     * bad CFA instruction and return falure */
    uint8_t opcodes[] = { DW_CFA_set_loc, 0x1, 0x2, 0x3, 0x4, DW_CFA_BAD_OPCODE};
    PERFORM_EVAL_TEST(opcodes, 0x1020304, PLCRASH_ESUCCESS);
    
    /* Test evaluation without GNU EH agumentation data (eg, using direct word sized pointers) */
    _cie.has_eh_augmentation = false;
    PERFORM_EVAL_TEST(opcodes, 0x1020304, PLCRASH_ESUCCESS);
}

/** Test evaluation of DW_CFA_advance_loc */
- (void) testAdvanceLoc {
    _cie.code_alignment_factor = 2;
    
    /* Evaluation should terminate prior to the bad opcode */
    uint8_t opcodes[] = { DW_CFA_advance_loc|0x1, DW_CFA_BAD_OPCODE};
    PERFORM_EVAL_TEST(opcodes, 0x2, PLCRASH_ESUCCESS);
}


/** Test evaluation of DW_CFA_advance_loc1 */
- (void) testAdvanceLoc1 {
    _cie.code_alignment_factor = 2;
    
    /* Evaluation should terminate prior to the bad opcode */
    uint8_t opcodes[] = { DW_CFA_advance_loc1, 0x1, DW_CFA_BAD_OPCODE};
    PERFORM_EVAL_TEST(opcodes, 0x2, PLCRASH_ESUCCESS);
}

/** Test evaluation of DW_CFA_advance_loc2 */
- (void) testAdvanceLoc2 {
    _cie.code_alignment_factor = 2;
    
    /* Evaluation should terminate prior to the bad opcode */
    uint8_t opcodes[] = { DW_CFA_advance_loc2, 0x0, 0x1, DW_CFA_BAD_OPCODE};
    PERFORM_EVAL_TEST(opcodes, 0x2, PLCRASH_ESUCCESS);
}

/** Test evaluation of DW_CFA_advance_loc2 */
- (void) testAdvanceLoc4 {
    _cie.code_alignment_factor = 2;
    
    /* Evaluation should terminate prior to the bad opcode */
    uint8_t opcodes[] = { DW_CFA_advance_loc4, 0x0, 0x0, 0x0, 0x1, DW_CFA_BAD_OPCODE};
    PERFORM_EVAL_TEST(opcodes, 0x2, PLCRASH_ESUCCESS);
}

/** Test evaluation of DW_CFA_def_cfa */
- (void) testDefineCFA {
    uint8_t opcodes[] = { DW_CFA_def_cfa, 0x1, 0x2};
    PERFORM_EVAL_TEST(opcodes, 0x0, PLCRASH_ESUCCESS);

    STAssertEquals(plcrash::DWARF_CFA_STATE_CFA_TYPE_REGISTER, _stack.get_cfa_rule().cfa_type, @"Unexpected CFA type");
    STAssertEquals((uint32_t)1, _stack.get_cfa_rule().reg.regnum, @"Unexpected CFA register");
    STAssertEquals((uint64_t)2, _stack.get_cfa_rule().reg.offset, @"Unexpected CFA offset");
}

/** Test evaluation of DW_CFA_def_cfa_sf */
- (void) testDefineCFASF {
    /* An alignment factor to be applied to the second operand. */
    _cie.data_alignment_factor = 2;

    uint8_t opcodes[] = { DW_CFA_def_cfa_sf, 0x1, 0x7e /* -2 */};
    PERFORM_EVAL_TEST(opcodes, 0x0, PLCRASH_ESUCCESS);

    STAssertEquals(plcrash::DWARF_CFA_STATE_CFA_TYPE_REGISTER_SIGNED, _stack.get_cfa_rule().cfa_type, @"Unexpected CFA type");
    STAssertEquals((uint32_t)1, _stack.get_cfa_rule().reg.regnum, @"Unexpected CFA register");
    STAssertEquals((int64_t)-4, _stack.get_cfa_rule().reg.signed_offset, @"Unexpected CFA offset");
}

/** Test evaluation of DW_CFA_def_cfa_register */
- (void) testDefineCFARegister {
    uint8_t opcodes[] = { DW_CFA_def_cfa, 0x1, 0x2, DW_CFA_def_cfa_register, 10 };
    
    /* Verify modification of unsigned state */
    PERFORM_EVAL_TEST(opcodes, 0x0, PLCRASH_ESUCCESS);

    STAssertEquals(plcrash::DWARF_CFA_STATE_CFA_TYPE_REGISTER, _stack.get_cfa_rule().cfa_type, @"Unexpected CFA type");
    STAssertEquals((uint32_t)10, _stack.get_cfa_rule().reg.regnum, @"Unexpected CFA register");
    STAssertEquals((uint64_t)2, _stack.get_cfa_rule().reg.offset, @"Unexpected CFA offset");
    
    /* Verify modification of signed state */
    opcodes[0] = DW_CFA_def_cfa_sf;
    opcodes[2] = 0x7e; /* -2 */
    PERFORM_EVAL_TEST(opcodes, 0x0, PLCRASH_ESUCCESS);
    
    STAssertEquals(plcrash::DWARF_CFA_STATE_CFA_TYPE_REGISTER_SIGNED, _stack.get_cfa_rule().cfa_type, @"Unexpected CFA type");
    STAssertEquals((uint32_t)10, _stack.get_cfa_rule().reg.regnum, @"Unexpected CFA register");
    STAssertEquals((int64_t)-2, _stack.get_cfa_rule().reg.signed_offset, @"Unexpected CFA offset");
    
    /* Verify behavior when a non-register CFA rule is present */
    _stack.set_cfa_expression(0);
    opcodes[0] = DW_CFA_nop;
    opcodes[1] = DW_CFA_nop;
    opcodes[2] = DW_CFA_nop;
    PERFORM_EVAL_TEST(opcodes, 0x0, PLCRASH_EINVAL);
}

/** Test evaluation of DW_CFA_def_cfa_offset */
- (void) testDefineCFAOffset {
    uint8_t opcodes[] = { DW_CFA_def_cfa, 0x1, 0x2, DW_CFA_def_cfa_offset, 10 };    
    PERFORM_EVAL_TEST(opcodes, 0x0, PLCRASH_ESUCCESS);
    
    STAssertEquals(plcrash::DWARF_CFA_STATE_CFA_TYPE_REGISTER, _stack.get_cfa_rule().cfa_type, @"Unexpected CFA type");
    STAssertEquals((uint32_t)1, _stack.get_cfa_rule().reg.regnum, @"Unexpected CFA register");
    STAssertEquals((uint64_t)10, _stack.get_cfa_rule().reg.offset, @"Unexpected CFA offset");

    /* Verify behavior when a non-register CFA rule is present */
    _stack.set_cfa_expression(0);
    opcodes[0] = DW_CFA_nop;
    opcodes[1] = DW_CFA_nop;
    opcodes[2] = DW_CFA_nop;
    PERFORM_EVAL_TEST(opcodes, 0x0, PLCRASH_EINVAL);
}

/** Test evaluation of DW_CFA_def_cfa_offset_sf */
- (void) testDefineCFAOffsetSF {
    /* An alignment factor to be applied to the signed offset operand. */
    _cie.data_alignment_factor = 2;

    uint8_t opcodes[] = { DW_CFA_def_cfa, 0x1, 0x2, DW_CFA_def_cfa_offset_sf, 0x7e /* -2 */ };
    PERFORM_EVAL_TEST(opcodes, 0x0, PLCRASH_ESUCCESS);
    
    STAssertEquals(plcrash::DWARF_CFA_STATE_CFA_TYPE_REGISTER_SIGNED, _stack.get_cfa_rule().cfa_type, @"Unexpected CFA type");
    STAssertEquals((uint32_t)1, _stack.get_cfa_rule().reg.regnum, @"Unexpected CFA register");
    STAssertEquals((int64_t)-4, _stack.get_cfa_rule().reg.signed_offset, @"Unexpected CFA offset");
    
    /* Verify behavior when a non-register CFA rule is present */
    _stack.set_cfa_expression(0);
    opcodes[0] = DW_CFA_nop;
    opcodes[1] = DW_CFA_nop;
    opcodes[2] = DW_CFA_nop;
    PERFORM_EVAL_TEST(opcodes, 0x0, PLCRASH_EINVAL);
}

/** Test evaluation of DW_CFA_def_cfa_expression */
- (void) testDefineCFAExpression {    
    uint8_t opcodes[] = { DW_CFA_def_cfa_expression, 0x1 /* 1 byte long */, DW_OP_nop /* expression opcodes */};
    PERFORM_EVAL_TEST(opcodes, 0x0, PLCRASH_ESUCCESS);
    
    STAssertEquals(plcrash::DWARF_CFA_STATE_CFA_TYPE_EXPRESSION, _stack.get_cfa_rule().cfa_type, @"Unexpected CFA type");
    STAssertEquals((pl_vm_address_t) &opcodes[1], _stack.get_cfa_rule().expression.address, @"Unexpected expression address");
}

/** Test evaluation of DW_CFA_undefined */
- (void) testUndefined {
    plcrash_dwarf_cfa_reg_rule_t rule;
    int64_t value;

    /* Define the register */
    _stack.set_register(1, PLCRASH_DWARF_CFA_REG_RULE_OFFSET, 10);
    STAssertTrue(_stack.get_register_rule(1, &rule, &value), @"Rule should be marked as defined");

    /* Perform undef */
    uint8_t opcodes[] = { DW_CFA_undefined, 0x1 };
    PERFORM_EVAL_TEST(opcodes, 0x0, PLCRASH_ESUCCESS);
    STAssertFalse(_stack.get_register_rule(1, &rule, &value), @"No rule should be defined for undef register");
}

/** Test evaluation of DW_CFA_same_value */
- (void) testSameValue {
    uint8_t opcodes[] = { DW_CFA_same_value, 0x1 };
    PERFORM_EVAL_TEST(opcodes, 0x0, PLCRASH_ESUCCESS);
    
    plcrash_dwarf_cfa_reg_rule_t rule;
    int64_t value;
    STAssertTrue(_stack.get_register_rule(1, &rule, &value), @"Failed to fetch rule");
    STAssertEquals(PLCRASH_DWARF_CFA_REG_RULE_SAME_VALUE, rule, @"Incorrect rule specified");
}

- (void) testBadOpcode {
    uint8_t opcodes[] = { DW_CFA_BAD_OPCODE };
    PERFORM_EVAL_TEST(opcodes, 0, PLCRASH_ENOTSUP);
}

/** Test basic evaluation of a NOP. */
- (void) testNop {
    uint8_t opcodes[] = { DW_CFA_nop, };
    
    PERFORM_EVAL_TEST(opcodes, 0, PLCRASH_ESUCCESS);
}

@end