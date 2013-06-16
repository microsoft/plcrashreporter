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

#ifndef PLCRASH_ASYNC_DWARF_CFA_H
#define PLCRASH_ASYNC_DWARF_CFA_H 1

#ifdef __cplusplus
extern "C" {
#endif

#include "PLCrashAsync.h"
#include "PLCrashAsyncMObject.h"
#include "PLCrashAsyncThread.h"

#include "PLCrashAsyncDwarfCIE.h"

/**
 * @internal
 * @ingroup plcrash_async_dwarf
 * @{
 */

/**
 * DWARF CFA opcodes, as defined by the DWARF 4 Specification (Section 7.23).
 *
 * The DWARF CFA opcodes are expressed in two forms; the first form includes an non-zero
 * opcode in the top two bits, with the bottom 6 bits used to provide a constant value
 * operand.
 *
 * The second form includes zeros in the top two bits, with the actual opcode
 * stored in the bottom 6 bits.
 */
typedef enum {
    /** DW_CFA_advance_loc: 'delta' stored in low six bits */
    DW_CFA_advance_loc = 0x40,
    
    /** DW_CFA_offset: 'register' stored in low six bits, operand is ULEB128 offset */
    DW_CFA_offset = 0x80,
    
    /** DW_CFA_restore: 'register' stored in low six bits */
    DW_CFA_restore = 0xc0,
    
    /** DW_CFA_nop */
    DW_CFA_nop = 0,
    
    /** DW_CFA_set_loc */
    DW_CFA_set_loc = 0x01,
    
    /** DW_CFA_advance_loc1, operand is 1-byte delta */
    DW_CFA_advance_loc1 = 0x02,
    
    /** DW_CFA_advance_loc2, operand is 2-byte delta */
    DW_CFA_advance_loc2 = 0x03,
    
    /** DW_CFA_advance_loc4, operand is 4-byte delta */
    DW_CFA_advance_loc4 = 0x04,
    
    /** DW_CFA_offset_extended, operands are ULEB128 register and ULEB128 offset */
    DW_CFA_offset_extended = 0x05,
    
    /** DW_CFA_restore_extended, operand is ULEB128 register */
    DW_CFA_restore_extended = 0x06,
    
    /** DW_CFA_undefined, operand is ULEB128 register */
    DW_CFA_undefined = 0x07,
    
    /** DW_CFA_same_value, operand is ULEB128 register */
    DW_CFA_same_value = 0x08,
    
    /** DW_CFA_register, operands are ULEB128 register, and ULEB128 register */
    DW_CFA_register = 0x09,
    
    /** DW_CFA_remember_state */
    DW_CFA_remember_state = 0x0a,
    
    /** DW_CFA_restore_state */
    DW_CFA_restore_state = 0x0b,
    
    /** DW_CFA_def_cfa, operands are ULEB128 register and ULEB128 offset */
    DW_CFA_def_cfa = 0x0c,
    
    /** DW_CFA_def_cfa_register, operand is ULEB128 register */
    DW_CFA_def_cfa_register = 0x0d,
    
    /** DW_CFA_def_cfa_offset, operand is ULEB128 offset */
    DW_CFA_def_cfa_offset = 0x0e,
    
    /** DW_CFA_def_cfa_expression */
    DW_CFA_def_cfa_expression = 0x0f,
    
    /** DW_CFA_expression, operands are ULEB128 register, BLOCK */
    DW_CFA_expression = 0x10,
    
    /** DW_CFA_offset_extended_sf, operands are ULEB128 register, SLEB128 offset */
    DW_CFA_offset_extended_sf = 0x11,
    
    /** DW_CFA_def_cfa_sf, operands are ULEB128, register SLEB128 offset */
    DW_CFA_def_cfa_sf = 0x12,
    
    /** DW_CFA_def_cfa_offset_sf, operand is SLEB128 offset */
    DW_CFA_def_cfa_offset_sf = 0x13,
    
    /** DW_CFA_val_offset, operands are ULEB128, ULEB128 */
    DW_CFA_val_offset = 0x14,
    
    /** DW_CFA_val_offset_sf, operands are ULEB128, SLEB128 */
    DW_CFA_val_offset_sf = 0x15,
    
    /** DW_CFA_val_expression, operands are ULEB128, BLOCK */
    DW_CFA_val_expression = 0x16,
    
    /** DW_CFA_lo_user */
    DW_CFA_lo_user = 0x1c,
    
    /** DW_CFA_hi_user */
    DW_CFA_hi_user = 0x3f,
} DW_CFA_t;

plcrash_error_t plcrash_async_dwarf_eval_cfa_program (plcrash_async_mobject_t *mobj,
                                                      pl_vm_address_t pc_offset,
                                                      plcrash_async_dwarf_cie_info_t *cie_info,
                                                      plcrash_async_dwarf_gnueh_ptr_state_t *ptr_state,
                                                      const plcrash_async_byteorder_t *byteorder,
                                                      pl_vm_address_t address,
                                                      pl_vm_off_t offset,
                                                      pl_vm_size_t length);

/**
 * @}
 */
    
#ifdef __cplusplus
}
#endif

#endif /* PLCRASH_ASYNC_DWARF_CFA_H */
