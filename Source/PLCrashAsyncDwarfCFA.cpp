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


#include "PLCrashAsyncDwarfCFA.hpp"
#include "dwarf_opstream.hpp"
#include <inttypes.h>

/**
 * @internal
 * @ingroup plcrash_async_dwarf
 * @{
 */

using namespace plcrash;

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
 * @param stack The CFA state stack to be used for evaluation of the CFA program. This state
 * may have been previously initialized by a common CFA initializer program.
 *
 * @return Returns PLCRASH_ESUCCESS on success, or an appropriate plcrash_error_t values
 * on failure. If an invalid opcode is detected, PLCRASH_ENOTSUP will be returned.
 *
 * @todo Consider defining updated status codes or error handling to provide more structured
 * error data on failure.
 */
plcrash_error_t plcrash_async_dwarf_eval_cfa_program (plcrash_async_mobject_t *mobj,
                                                      pl_vm_address_t pc_offset,
                                                      plcrash_async_dwarf_cie_info_t *cie_info,
                                                      plcrash_async_dwarf_gnueh_ptr_state_t *ptr_state,
                                                      const plcrash_async_byteorder_t *byteorder,
                                                      pl_vm_address_t address,
                                                      pl_vm_off_t offset,
                                                      pl_vm_size_t length,
                                                      plcrash::dwarf_cfa_state *stack)
{
    dwarf_opstream opstream;
    plcrash_error_t err;
    pl_vm_address_t location = 0;

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
                stack->set_cfa_register(dw_expr_read_uleb128_regnum(), DWARF_CFA_STATE_CFA_TYPE_REGISTER, dw_expr_read_uleb128());
                break;
                
            case DW_CFA_def_cfa_sf:
                stack->set_cfa_register(dw_expr_read_uleb128_regnum(), DWARF_CFA_STATE_CFA_TYPE_REGISTER_SIGNED, (uint64_t) (dw_expr_read_sleb128() * cie_info->data_alignment_factor));
                break;
                
            case DW_CFA_def_cfa_register: {
                dwarf_cfa_state::dwarf_cfa_rule_t rule = stack->get_cfa_rule();
                
                switch (rule.cfa_type) {
                    case DWARF_CFA_STATE_CFA_TYPE_REGISTER:
                    case DWARF_CFA_STATE_CFA_TYPE_REGISTER_SIGNED:
                        stack->set_cfa_register(dw_expr_read_uleb128_regnum(), rule.cfa_type, rule.reg.offset);
                        break;
                        
                    case DWARF_CFA_STATE_CFA_TYPE_EXPRESSION:
                    case DWARF_CFA_STATE_CFA_TYPE_UNDEFINED:
                        PLCF_DEBUG("DW_CFA_def_cfa_register emitted for a non-register CFA rule state");
                        return PLCRASH_EINVAL;
                }
                break;
            }
                
            case DW_CFA_def_cfa_offset: {
                dwarf_cfa_state::dwarf_cfa_rule_t rule = stack->get_cfa_rule();
                switch (rule.cfa_type) {
                    case DWARF_CFA_STATE_CFA_TYPE_REGISTER:
                    case DWARF_CFA_STATE_CFA_TYPE_REGISTER_SIGNED:
                        /* Our new offset is unsigned, so all register rules are converted to unsigned here */
                        stack->set_cfa_register(rule.reg.regnum, DWARF_CFA_STATE_CFA_TYPE_REGISTER, dw_expr_read_uleb128());
                        break;
                        
                    case DWARF_CFA_STATE_CFA_TYPE_EXPRESSION:
                    case DWARF_CFA_STATE_CFA_TYPE_UNDEFINED:
                        PLCF_DEBUG("DW_CFA_def_cfa_register emitted for a non-register CFA rule state");
                        return PLCRASH_EINVAL;
                }
                break;
            }
                
                
            case DW_CFA_def_cfa_offset_sf: {
                dwarf_cfa_state::dwarf_cfa_rule_t rule = stack->get_cfa_rule();
                switch (rule.cfa_type) {
                    case DWARF_CFA_STATE_CFA_TYPE_REGISTER:
                    case DWARF_CFA_STATE_CFA_TYPE_REGISTER_SIGNED:
                        /* Our new offset is signed, so all register rules are converted to signed here */
                        stack->set_cfa_register(rule.reg.regnum, DWARF_CFA_STATE_CFA_TYPE_REGISTER_SIGNED, (uint64_t)(dw_expr_read_sleb128() * cie_info->data_alignment_factor));
                        break;

                    case DWARF_CFA_STATE_CFA_TYPE_EXPRESSION:
                    case DWARF_CFA_STATE_CFA_TYPE_UNDEFINED:
                        PLCF_DEBUG("DW_CFA_def_cfa_register emitted for a non-register CFA rule state");
                        return PLCRASH_EINVAL;
                }
                break;
            }

            case DW_CFA_def_cfa_expression: {
                uintptr_t pos = opstream.get_position();
                
                /* Fetch the DWARF_FORM_block length header; we need this to skip the over the DWARF expression. */
                uint64_t length = dw_expr_read_uleb128();

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
                stack->set_cfa_expression(abs_addr);
                
                /* Skip the expression opcodes */
                opstream.skip(length);
                break;
            }
                
            case DW_CFA_undefined:
                stack->remove_register(dw_expr_read_uleb128_regnum());
                break;
                
            case DW_CFA_same_value:
                stack->set_register(dw_expr_read_uleb128_regnum(), PLCRASH_DWARF_CFA_REG_RULE_SAME_VALUE, 0);
                break;
                
            case DW_CFA_offset:
                stack->set_register(const_operand, PLCRASH_DWARF_CFA_REG_RULE_OFFSET, dw_expr_read_uleb128() * cie_info->data_alignment_factor);
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
 * @}
 */