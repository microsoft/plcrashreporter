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

#include "PLCrashAsyncDwarfExpression.h"

#include "PLCrashAsyncDwarfCFA.hpp"
#include "dwarf_opstream.hpp"
#include <inttypes.h>

/**
 * @internal
 * @ingroup plcrash_async_dwarf
 * @{
 */

using namespace plcrash::async;

/**
 * Evaluate a DWARF CFA program, as defined in the DWARF 4 Specification, Section 6.4.2. This
 * internal implementation is templated to support 32-bit and 64-bit evaluation.
 *
 * @param mobj The memory object from which the expression opcodes will be read.
 * @param pc_offset The PC offset at which evaluation of the CFA program should terminate. If 0, 
 * the program will be executed to completion. This offset should be relative to the executable image's
 * base address.
 * @param cie_info The CIE info data for this opcode stream.
 * @param ptr_state GNU EH pointer state configuration; this defines the base addresses and other
 * information required to decode pointers in the CFA opcode stream. May be NULL if eh_frame
 * augmentation data is not available in @a cie_info.
 * @param byteoder The byte order of the data referenced by @a mobj.
 * @param address The task-relative address within @a mobj at which the opcodes will be fetched.
 * @param offset An offset to be applied to @a address.
 * @param length The total length of the opcodes readable at @a address + @a offset.
 * @param state The CFA state stack to be used for evaluation of the CFA program. This state
 * may have been previously initialized by a common CFA initializer program, and will be used
 * as the initial state when evaluating opcodes such as DW_CFA_restore.
 *
 * @return Returns PLCRASH_ESUCCESS on success, or an appropriate plcrash_error_t values
 * on failure. If an invalid opcode is detected, PLCRASH_ENOTSUP will be returned.
 *
 * @todo Consider defining updated status codes or error handling to provide more structured
 * error data on failure.
 */
plcrash_error_t plcrash_async_dwarf_cfa_eval_program (plcrash_async_mobject_t *mobj,
                                                      pl_vm_address_t pc_offset,
                                                      plcrash_async_dwarf_cie_info_t *cie_info,
                                                      plcrash_async_dwarf_gnueh_ptr_state_t *ptr_state,
                                                      const plcrash_async_byteorder_t *byteorder,
                                                      pl_vm_address_t address,
                                                      pl_vm_off_t offset,
                                                      pl_vm_size_t length,
                                                      plcrash::async::dwarf_cfa_state *state)
{
    plcrash::async::dwarf_opstream opstream;
    plcrash_error_t err;
    uint64_t location = 0;

    /* Save the initial state; this is needed for DW_CFA_restore, et al. */
    // TODO - It would be preferrable to only allocate the number of registers actually required here.
    dwarf_cfa_state initial_state;
    {
        dwarf_cfa_state_regnum_t regnum;
        plcrash_dwarf_cfa_reg_rule_t rule;
        uint64_t value;

        dwarf_cfa_state_iterator iter = dwarf_cfa_state_iterator(state);
        while (iter.next(&regnum, &rule, &value)) {
            if (!initial_state.set_register(regnum, rule, value)) {
                PLCF_DEBUG("Hit register allocation limit while saving initial state");
                return PLCRASH_ENOMEM;
            }
        }
    }

    /* Default to reading as a standard machine word */
    DW_EH_PE_t gnu_eh_ptr_encoding = DW_EH_PE_absptr;
    if (cie_info->has_eh_augmentation && cie_info->eh_augmentation.has_pointer_encoding && ptr_state != NULL) {
        gnu_eh_ptr_encoding = (DW_EH_PE_t) cie_info->eh_augmentation.pointer_encoding;
    }
    
    /* Calculate the absolute (target-relative) address of the start of the stream */
    pl_vm_address_t opstream_target_address;
    if (!plcrash_async_address_apply_offset(address, offset, &opstream_target_address)) {
        PLCF_DEBUG("Offset overflows base address");
        return PLCRASH_EINVAL;
    }

    /* Configure the opstream */
    if ((err = opstream.init(mobj, byteorder, address, offset, length)) != PLCRASH_ESUCCESS)
        return err;
    
#define dw_expr_read_int(_type) ({ \
    _type v; \
    if (!opstream.read_intU<_type>(&v)) { \
        PLCF_DEBUG("Read of size %zu exceeds mapped range", sizeof(v)); \
        return PLCRASH_EINVAL; \
    } \
    v; \
})
    
    /* A position-advancing DWARF uleb128 register read macro that uses GCC/clang's compound statement value extension, returning an error
     * if the read fails, or the register value exceeds DWARF_CFA_STATE_REGNUM_MAX */
#define dw_expr_read_uleb128_regnum() ({ \
    uint64_t v; \
    if (!opstream.read_uleb128(&v)) { \
        PLCF_DEBUG("Read of ULEB128 value failed"); \
        return PLCRASH_EINVAL; \
    } \
    if (v > DWARF_CFA_STATE_REGNUM_MAX) { \
        PLCF_DEBUG("Register number %" PRIu64 " exceeds DWARF_CFA_STATE_REGNUM_MAX", v); \
        return PLCRASH_ENOTSUP; \
    } \
    (uint32_t) v; \
})
    
    /* A position-advancing uleb128 read macro that uses GCC/clang's compound statement value extension, returning an error
     * if the read fails. */
#define dw_expr_read_uleb128() ({ \
    uint64_t v; \
    if (!opstream.read_uleb128(&v)) { \
        PLCF_DEBUG("Read of ULEB128 value failed"); \
        return PLCRASH_EINVAL; \
    } \
    v; \
})

    /* A position-advancing sleb128 read macro that uses GCC/clang's compound statement value extension, returning an error
     * if the read fails. */
#define dw_expr_read_sleb128() ({ \
    int64_t v; \
    if (!opstream.read_sleb128(&v)) { \
        PLCF_DEBUG("Read of SLEB128 value failed"); \
        return PLCRASH_EINVAL; \
    } \
    v; \
})
    
    /* Handle error checking when setting a register on the CFA state */
#define dw_expr_set_register(_regnum, _rule, _value) do { \
    if (!state->set_register(_regnum, _rule, _value)) { \
        PLCF_DEBUG("Exhausted available register slots while evaluating CFA opcodes"); \
        return PLCRASH_ENOMEM; \
    } \
} while (0)

    /* Iterate the opcode stream until the pc_offset is hit */
    uint8_t opcode;
    while ((pc_offset == 0 || location < pc_offset) && opstream.read_intU(&opcode)) {
        uint8_t const_operand = 0;

        /* Check for opcodes encoded in the top two bits, with an operand
         * in the bottom 6 bits. */
        
        if ((opcode & 0xC0) != 0) {
            const_operand = opcode & 0x3F;
            opcode &= 0xC0;
        }
        
        switch (opcode) {
            case DW_CFA_set_loc:
                if (cie_info->segment_size != 0) {
                    PLCF_DEBUG("Segment support has not been implemented");
                    return PLCRASH_ENOTSUP;
                }

                /* Try reading an eh_frame encoded pointer */
                if (!opstream.read_gnueh_ptr(ptr_state, gnu_eh_ptr_encoding, &location)) {
                    PLCF_DEBUG("DW_CFA_set_loc failed to read the target pointer value");
                    return PLCRASH_EINVAL;
                }
                break;
                
            case DW_CFA_advance_loc:
                location += const_operand * cie_info->code_alignment_factor;
                break;
                
            case DW_CFA_advance_loc1:
                location += dw_expr_read_int(uint8_t) * cie_info->code_alignment_factor;
                break;
                
            case DW_CFA_advance_loc2:
                location += dw_expr_read_int(uint16_t) * cie_info->code_alignment_factor;
                break;
                
            case DW_CFA_advance_loc4:
                location += dw_expr_read_int(uint32_t) * cie_info->code_alignment_factor;
                break;
                
            case DW_CFA_def_cfa:
                state->set_cfa_register(dw_expr_read_uleb128_regnum(), DWARF_CFA_STATE_CFA_TYPE_REGISTER, dw_expr_read_uleb128());
                break;
                
            case DW_CFA_def_cfa_sf:
                state->set_cfa_register(dw_expr_read_uleb128_regnum(), DWARF_CFA_STATE_CFA_TYPE_REGISTER_SIGNED, dw_expr_read_sleb128() * cie_info->data_alignment_factor);
                break;
                
            case DW_CFA_def_cfa_register: {
                dwarf_cfa_rule_t rule = state->get_cfa_rule();
                
                switch (rule.cfa_type) {
                    case DWARF_CFA_STATE_CFA_TYPE_REGISTER:
                    case DWARF_CFA_STATE_CFA_TYPE_REGISTER_SIGNED:
                        state->set_cfa_register(dw_expr_read_uleb128_regnum(), rule.cfa_type, rule.reg.offset);
                        break;
                        
                    case DWARF_CFA_STATE_CFA_TYPE_EXPRESSION:
                    case DWARF_CFA_STATE_CFA_TYPE_UNDEFINED:
                        PLCF_DEBUG("DW_CFA_def_cfa_register emitted for a non-register CFA rule state");
                        return PLCRASH_EINVAL;
                }
                break;
            }
                
            case DW_CFA_def_cfa_offset: {
                dwarf_cfa_rule_t rule = state->get_cfa_rule();
                switch (rule.cfa_type) {
                    case DWARF_CFA_STATE_CFA_TYPE_REGISTER:
                    case DWARF_CFA_STATE_CFA_TYPE_REGISTER_SIGNED:
                        /* Our new offset is unsigned, so all register rules are converted to unsigned here */
                        state->set_cfa_register(rule.reg.regnum, DWARF_CFA_STATE_CFA_TYPE_REGISTER, dw_expr_read_uleb128());
                        break;
                        
                    case DWARF_CFA_STATE_CFA_TYPE_EXPRESSION:
                    case DWARF_CFA_STATE_CFA_TYPE_UNDEFINED:
                        PLCF_DEBUG("DW_CFA_def_cfa_register emitted for a non-register CFA rule state");
                        return PLCRASH_EINVAL;
                }
                break;
            }
                
                
            case DW_CFA_def_cfa_offset_sf: {
                dwarf_cfa_rule_t rule = state->get_cfa_rule();
                switch (rule.cfa_type) {
                    case DWARF_CFA_STATE_CFA_TYPE_REGISTER:
                    case DWARF_CFA_STATE_CFA_TYPE_REGISTER_SIGNED:
                        /* Our new offset is signed, so all register rules are converted to signed here */
                        state->set_cfa_register(rule.reg.regnum, DWARF_CFA_STATE_CFA_TYPE_REGISTER_SIGNED, dw_expr_read_sleb128() * cie_info->data_alignment_factor);
                        break;

                    case DWARF_CFA_STATE_CFA_TYPE_EXPRESSION:
                    case DWARF_CFA_STATE_CFA_TYPE_UNDEFINED:
                        PLCF_DEBUG("DW_CFA_def_cfa_register emitted for a non-register CFA rule state");
                        return PLCRASH_EINVAL;
                }
                break;
            }

            case DW_CFA_def_cfa_expression: {                
                /* Fetch the DWARF_FORM_block length header; we need this to skip the over the DWARF expression. */
                uint64_t length = dw_expr_read_uleb128();
                
                /* Fetch the opstream position of the DWARF expression */
                uintptr_t pos = opstream.get_position();

                /* The returned sizes should always fit within the VM types in valid DWARF data; if they don't, how
                 * are we debugging the target? */
                if (length > PL_VM_SIZE_MAX || length > PL_VM_OFF_MAX) {
                    PLCF_DEBUG("DWARF expression length exceeds PL_VM_SIZE_MAX/PL_VM_OFF_MAX in DW_CFA_def_cfa_expression operand");
                    return PLCRASH_ENOTSUP;
                }
                
                if (pos > PL_VM_ADDRESS_MAX || pos > PL_VM_OFF_MAX) {
                    PLCF_DEBUG("DWARF expression position exceeds PL_VM_ADDRESS_MAX/PL_VM_OFF_MAX in CFA opcode stream");
                    return PLCRASH_ENOTSUP;
                }
                
                /* Calculate the absolute address of the expression opcodes. */
                pl_vm_address_t abs_addr;
                if (!plcrash_async_address_apply_offset(opstream_target_address, pos, &abs_addr)) {
                    PLCF_DEBUG("Offset overflows base address");
                    return PLCRASH_EINVAL;
                }

                /* Save the position */
                state->set_cfa_expression(abs_addr, length);
                
                /* Skip the expression opcodes */
                opstream.skip(length);
                break;
            }
                
            case DW_CFA_undefined:
                state->remove_register(dw_expr_read_uleb128_regnum());
                break;
                
            case DW_CFA_same_value:
                dw_expr_set_register(dw_expr_read_uleb128_regnum(), PLCRASH_DWARF_CFA_REG_RULE_SAME_VALUE, 0);
                break;
                
            case DW_CFA_offset:
                dw_expr_set_register(const_operand, PLCRASH_DWARF_CFA_REG_RULE_OFFSET, dw_expr_read_uleb128() * cie_info->data_alignment_factor);
                break;
                
            case DW_CFA_offset_extended:
                dw_expr_set_register(dw_expr_read_uleb128_regnum(), PLCRASH_DWARF_CFA_REG_RULE_OFFSET, dw_expr_read_uleb128() * cie_info->data_alignment_factor);
                break;
                
            case DW_CFA_offset_extended_sf:
                dw_expr_set_register(dw_expr_read_uleb128_regnum(), PLCRASH_DWARF_CFA_REG_RULE_OFFSET, dw_expr_read_sleb128() * cie_info->data_alignment_factor);
                break;
                
            case DW_CFA_val_offset:
                dw_expr_set_register(dw_expr_read_uleb128_regnum(), PLCRASH_DWARF_CFA_REG_RULE_VAL_OFFSET, dw_expr_read_uleb128() * cie_info->data_alignment_factor);
                break;
                
            case DW_CFA_val_offset_sf:
                dw_expr_set_register(dw_expr_read_uleb128_regnum(), PLCRASH_DWARF_CFA_REG_RULE_VAL_OFFSET, dw_expr_read_sleb128() * cie_info->data_alignment_factor);
                break;
                
            case DW_CFA_register:
                dw_expr_set_register(dw_expr_read_uleb128_regnum(), PLCRASH_DWARF_CFA_REG_RULE_REGISTER, dw_expr_read_uleb128());
                break;
            
            case DW_CFA_expression:
            case DW_CFA_val_expression: {
                dwarf_cfa_state_regnum_t regnum = dw_expr_read_uleb128_regnum();
                uintptr_t pos = opstream.get_position();
                
                /* Fetch the DWARF_FORM_block length header; we need this to skip the over the DWARF expression. */
                uint64_t length = dw_expr_read_uleb128();

                /* Calculate the absolute address of the expression opcodes (including verifying that pos won't overflow when applying the offset). */
                if (pos > PL_VM_ADDRESS_MAX || pos > PL_VM_OFF_MAX) {
                    PLCF_DEBUG("DWARF expression position exceeds PL_VM_ADDRESS_MAX/PL_VM_OFF_MAX in DW_CFA_expression evaluation");
                    return PLCRASH_ENOTSUP;
                }
                
                pl_vm_address_t abs_addr;
                if (!plcrash_async_address_apply_offset(opstream_target_address, pos, &abs_addr)) {
                    PLCF_DEBUG("Offset overflows base address");
                    return PLCRASH_EINVAL;
                }
                
                /* Save the position */
                if (opcode == DW_CFA_expression) {
                    dw_expr_set_register(regnum, PLCRASH_DWARF_CFA_REG_RULE_EXPRESSION, (int64_t)abs_addr);
                } else {
                    PLCF_ASSERT(opcode == DW_CFA_val_expression); // If not _expression, must be _val_expression.
                    dw_expr_set_register(regnum, PLCRASH_DWARF_CFA_REG_RULE_VAL_EXPRESSION, (int64_t)abs_addr);
                }

                /* Skip the expression opcodes */
                opstream.skip(length);
                break;
            }
                
            case DW_CFA_restore: {
                plcrash_dwarf_cfa_reg_rule_t rule;
                int64_t value;
                
                /* Either restore the value specified in the initial state, or remove the register
                 * if the initial state has no associated value */
                if (initial_state.get_register_rule(const_operand, &rule, &value)) {
                    dw_expr_set_register(const_operand, rule, value);
                } else {
                    state->remove_register(const_operand);
                }
        
                break;
            }
                
            case DW_CFA_restore_extended: {
                dwarf_cfa_state_regnum_t regnum = dw_expr_read_uleb128_regnum();
                plcrash_dwarf_cfa_reg_rule_t rule;
                int64_t value;
                
                /* Either restore the value specified in the initial state, or remove the register
                 * if the initial state has no associated value */
                if (initial_state.get_register_rule(regnum, &rule, &value)) {
                    dw_expr_set_register(regnum, rule, value);
                } else {
                    state->remove_register(const_operand);
                }
                
                break;
            }
                
            case DW_CFA_remember_state:
                if (!state->push_state()) {
                    PLCF_DEBUG("DW_CFA_remember_state exeeded the allocated CFA stack size");
                    return PLCRASH_ENOMEM;
                }
                break;
                
            case DW_CFA_restore_state:
                if (!state->pop_state()) {
                    PLCF_DEBUG("DW_CFA_restore_state was issued on an empty CFA stack");
                    return PLCRASH_EINVAL;
                }
                break;

            case DW_CFA_nop:
                break;
                
            default:
                PLCF_DEBUG("Unsupported opcode 0x%" PRIx8, opcode);
                return PLCRASH_ENOTSUP;
        }
    }

    return PLCRASH_ESUCCESS;
}

/**
 * Apply the evaluated @a cfa_state to @a thread_state, fetching data from @a task, and
 * populate @a new_thread_state with the result.
 *
 * @param task The task containing any data referenced by @a thread_state.
 * @param thread_state The current thread state corresponding to @a entry.
 * @param byteorder The target's byte order.
 * @param cfa_state CFA evaluation state as generated from evaluating a CFA program. @sa plcrash_async_dwarf_cfa_eval_program.
 * @param new_thread_state The new thread state to be initialized.
 *
 * @return Returns PLCRASH_ESUCCESS on success, or a standard pclrash_error_t code if an error occurs.
 */
plcrash_error_t plcrash_async_dwarf_cfa_state_apply (task_t task,
                                                     const plcrash_async_thread_state_t *thread_state,
                                                     const plcrash_async_byteorder_t *byteorder,
                                                     plcrash::async::dwarf_cfa_state *cfa_state,
                                                     plcrash_async_thread_state_t *new_thread_state)
{
    plcrash_error_t err;

    /* Initialize the new thread state */
    plcrash_async_thread_state_copy(new_thread_state, thread_state);
    plcrash_async_thread_state_clear_all_regs(new_thread_state);
    
    /* Determine the register width */
    bool m64 = false;
    pl_vm_address_t greg_size = plcrash_async_thread_state_get_greg_size(thread_state);
    switch (greg_size) {
        case 4:
            break;
        case 8:
            m64 = true;
            break;
        default:
            PLCF_DEBUG("Unsupported register width!");
            return PLCRASH_ENOTSUP;
    }

    /*
     * Restore the canonical frame address
     */
    dwarf_cfa_rule_t cfa_rule = cfa_state->get_cfa_rule();
    pl_vm_address_t cfa_val;

    switch (cfa_rule.cfa_type) {
        case DWARF_CFA_STATE_CFA_TYPE_UNDEFINED:
            /** Missing canonical frame address! */
            PLCF_DEBUG("No canonical frame address specified in the CFA state; can't apply state");
            return PLCRASH_EINVAL;

        case DWARF_CFA_STATE_CFA_TYPE_REGISTER:
        case DWARF_CFA_STATE_CFA_TYPE_REGISTER_SIGNED: {
            plcrash_regnum_t regnum;
            
            /* Map to a plcrash register number */
            if (!plcrash_async_thread_state_map_dwarf_to_reg(thread_state, cfa_rule.reg.regnum, &regnum)) {
                PLCF_DEBUG("CFA rule references an unsupported DWARF register: 0x%" PRIx32, cfa_rule.reg.regnum);
                return PLCRASH_ENOTSUP;
            }
            
            /* Verify that the requested register is available */
            if (!plcrash_async_thread_state_has_reg(thread_state, regnum)) {
                PLCF_DEBUG("CFA rule references a register that is not available from the current thread state: %s", plcrash_async_thread_state_get_reg_name(thread_state, regnum));
                return PLCRASH_ENOTFOUND;
            }

            /* Fetch the current value, apply the offset, and save as the new thread's CFA. */
            cfa_val = plcrash_async_thread_state_get_reg(thread_state, regnum);
            if (cfa_rule.cfa_type == DWARF_CFA_STATE_CFA_TYPE_REGISTER)
                cfa_val += ((uint64_t) cfa_rule.reg.offset);
            else
                cfa_val += cfa_rule.reg.offset;
            break;
        }

        case DWARF_CFA_STATE_CFA_TYPE_EXPRESSION: {
            plcrash_async_mobject_t mobj;
            if ((err = plcrash_async_mobject_init(&mobj, task, cfa_rule.expression.address, cfa_rule.expression.length, true)) != PLCRASH_ESUCCESS) {
                PLCF_DEBUG("Could not map CFA expression range");
                return err;
            }

            if (m64) {
                uint64_t result;
                if ((err = plcrash_async_dwarf_expression_eval_64(&mobj, task, thread_state, byteorder, cfa_rule.expression.address, 0x0, cfa_rule.expression.length, &result)) != PLCRASH_ESUCCESS) {
                    PLCF_DEBUG("CFA eval_64 failed");
                    return err;
                }

                cfa_val = result;
            } else {
                uint32_t result;
                if ((err = plcrash_async_dwarf_expression_eval_32(&mobj, task, thread_state, byteorder, cfa_rule.expression.address, 0x0, cfa_rule.expression.length, &result)) != PLCRASH_ESUCCESS) {
                    PLCF_DEBUG("CFA eval_32 failed");
                    return err;
                }
                
                cfa_val = result;
            }
            
            
            break;
        }
    }
    
    /* Apply the CFA to the new state */
    plcrash_async_thread_state_set_reg(new_thread_state, PLCRASH_REG_SP, cfa_val);
    
    /*
     * Restore register values
     */
    dwarf_cfa_state_iterator iter = dwarf_cfa_state_iterator(cfa_state);
    dwarf_cfa_state_regnum_t dw_regnum;
    plcrash_dwarf_cfa_reg_rule_t dw_rule;
    uint64_t dw_value;
    
    while (iter.next(&dw_regnum, &dw_rule, &dw_value)) {
        union {
            uint32_t v32;
            uint64_t v64;
        } rvalue;
        void *vptr = &rvalue;
    
        /* Map the register number */
        plcrash_regnum_t pl_regnum;
        if (!plcrash_async_thread_state_map_dwarf_to_reg(thread_state, dw_regnum, &pl_regnum)) {
            PLCF_DEBUG("Register rule references an unsupported DWARF register: 0x%" PRIx64, (uint64_t) dw_regnum);
            return PLCRASH_EINVAL;
        }
        
        /* Apply the rule */
        switch (dw_rule) {
            case PLCRASH_DWARF_CFA_REG_RULE_OFFSET: {
                if ((err = plcrash_async_safe_memcpy(task, cfa_val, (int64_t)dw_value, vptr, greg_size)) != PLCRASH_ESUCCESS) {
                    PLCF_DEBUG("Failed to read offset(N) register value: %d", err);
                    return err;
                }
                
                if (m64) {
                    plcrash_async_thread_state_set_reg(new_thread_state, pl_regnum, rvalue.v64);
                } else {
                    plcrash_async_thread_state_set_reg(new_thread_state, pl_regnum, rvalue.v32);
                }

                break;
            }

            case PLCRASH_DWARF_CFA_REG_RULE_VAL_OFFSET:
                plcrash_async_thread_state_set_reg(new_thread_state, pl_regnum, cfa_val + ((int64_t) dw_value));
                break;

            case PLCRASH_DWARF_CFA_REG_RULE_REGISTER: {
                /* The previous value of this register is stored in another register numbered R. */
                plcrash_regnum_t src_pl_regnum;
                if (!plcrash_async_thread_state_map_dwarf_to_reg(thread_state, dw_value, &src_pl_regnum)) {
                    PLCF_DEBUG("Register rule references an unsupported DWARF register: 0x%" PRIx64, (uint64_t) dw_regnum);
                    return PLCRASH_EINVAL;
                }
                
                if (!plcrash_async_thread_state_has_reg(thread_state, src_pl_regnum)) {
                    PLCF_DEBUG("Register rule references a register that is not available from the current thread state: %s", plcrash_async_thread_state_get_reg_name(thread_state, src_pl_regnum));
                    return PLCRASH_ENOTFOUND;
                }
                
                plcrash_async_thread_state_set_reg(new_thread_state, pl_regnum, plcrash_async_thread_state_get_reg(thread_state, src_pl_regnum));
                break;
            }
                
            case PLCRASH_DWARF_CFA_REG_RULE_EXPRESSION:
                /* The previous value of this register is located at the address produced by executing the DWARF expression E. */
                // TODO
                return PLCRASH_ENOTSUP;
                
            case PLCRASH_DWARF_CFA_REG_RULE_VAL_EXPRESSION:
                /* The previous value of this register is the value produced by executing the DWARF expression E. */
                // TODO
                return PLCRASH_ENOTSUP;
                
            case PLCRASH_DWARF_CFA_REG_RULE_SAME_VALUE:
                /* This register has not been modified from the previous frame. (By convention, it is preserved by the callee, but
                 * the callee has not modified it.)
                 *
                 * The register's value may be found in the frame's thread state. For frames other than the first, the
                 * register may not have been restored, and thus may be unavailable. */
                // TODO
                return PLCRASH_ENOTSUP;
        }
    }

    return PLCRASH_ESUCCESS;
}

/**
 * @}
 */