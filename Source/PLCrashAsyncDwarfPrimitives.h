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

#ifndef PLCRASH_ASYNC_DWARF_PRIVATE_H
#define PLCRASH_ASYNC_DWARF_PRIVATE_H 1

#include "PLCrashAsync.h"
#include "PLCrashAsyncImageList.h"
#include "PLCrashAsyncThread.h"

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

/**
 * Exception handling pointer encoding constants, as defined by the LSB Specification:
 * http://refspecs.linuxfoundation.org/LSB_4.1.0/LSB-Core-generic/LSB-Core-generic/dwarfext.html
 *
 * The upper 4 bits indicate how the value is to be applied. The lower 4 bits indicate the encoding format of the data.
 */
typedef enum DW_EH_PE {
    /**
     * Value is an indirect reference. This value is not specified by the LSB, and appears to be a
     * GCC extension; unfortunately, the intended use is not clear:
     *
     * - Apple's implementation of libunwind treats this as an indirected reference to a target-width pointer value,
     *   as does the upstream libunwind.
     * - LLDB does not appear to support indirect encoding at all.
     * - LLVM's asm printer decodes it as an independent flag on the encoding type value; eg, DW_EH_PE_indirect | DW_EH_PE_uleb128 | DW_EH_PE_pcrel
     *   LLVM/clang does not seem to otherwise emit this value.
     * - GDB explicitly does not support indirect encodings.
     *
     * For our purposes, we treat the value as per LLVM's asm printer, and may re-evaluate if the indirect encoding
     * is ever seen in the wild.
     */
    DW_EH_PE_indirect = 0x80,
    
    /** No value is present. */
    DW_EH_PE_omit = 0xff,
    
    /**
     * If no flags are set (0x0), the value is a literal pointer whose size is determined by the architecture. Note that this has two different meanings,
     * and is not a flag, but rather, the absense of a flag (0x0). If a relative flag is not set in the high 4 bits of the DW_EH_PE encoding, this
     * signifies that no offset is to be applied. If the bottom 4 bits are 0, this signifies a native-width pointer value reference.
     *
     * As such, it's possible to mix DW_EH_PE_absptr with other relative flags, eg, DW_EH_PE_textrel. Examples:
     *
     * - DW_EH_PE_absptr|DW_EH_PE_textrel (0x20): Address is relative to TEXT segment, and is an architecture-native pointer width.
     * - DW_EH_PE_absptr|DW_EH_PE_uleb128 (0x1): The address is absolute, and is a ULEB128 value.
     * - DW_EH_PE_absptr|DW_EH_PE_uleb128|DW_EH_PE_indirect (0x81): The address is absolute, indirect, and is a ULEB128 value.
     */
    DW_EH_PE_absptr = 0x00,
    
    /** Unsigned value encoded using LEB128 as defined by DWARF Debugging Information Format, Revision 2.0.0. */
    DW_EH_PE_uleb128 = 0x01,
    
    /** Unsigned 16-bit value */
    DW_EH_PE_udata2 = 0x02,
    
    /* Unsigned 32-bit value */
    DW_EH_PE_udata4 = 0x03,
    
    /** Unsigned 64-bit value */
    DW_EH_PE_udata8 = 0x04,
    
    /** Signed value encoded using LEB128 as defined by DWARF Debugging Information Format, Revision 2.0.0. */
    DW_EH_PE_sleb128 = 0x09,
    
    /** Signed 16-bit value */
    DW_EH_PE_sdata2 = 0x0a,
    
    /** Signed 32-bit value */
    DW_EH_PE_sdata4 = 0x0b,
    
    /** Signed 64-bit value */
    DW_EH_PE_sdata8 = 0x0c,
    
    /** Value is relative to the current program counter. */
    DW_EH_PE_pcrel = 0x10,
    
    /** Value is relative to the beginning of the __TEXT section. */
    DW_EH_PE_textrel = 0x20,
    
    /** Value is relative to the beginning of the __DATA section. */
    DW_EH_PE_datarel = 0x30,
    
    /** Value is relative to the beginning of the function. */
    DW_EH_PE_funcrel = 0x40,
    
    /**
     * Value is aligned to an address unit sized boundary. The meaning of this flag is not defined in the
     * LSB 4.1.0 specification; review of various implementations demonstrate that:
     *
     * - The value must be aligned relative to the VM load address of the eh_frame/debug_frame section that contains
     *   it.
     * - Some implementations assume that an aligned pointer value is always the architecture's natural pointer size.
     *   Other implementations, such as gdb, permit the use of alternative value types (uleb, sleb, data2/4/8, etc).
     *
     * In our implementation, we support the combination of DW_EH_PE_aligned with any other supported value type.
     */
    DW_EH_PE_aligned = 0x50,
} DW_EH_PE_t;

/** Mask for the lower four bits of a DW_EH_PE_t value, defining the encoding type. */
#define DW_EH_PE_MASK_ENCODING 0x0F

/**
 * DWARF CFA register rules, as defined in DWARF 4 Section 6.4.1.
 */
typedef enum {
    /**
     * The previous value of this register is saved at the address CFA+N where CFA is the current
     * CFA value and N is a signed offset.
     */
    PLCRASH_DWARF_CFA_REG_RULE_OFFSET = 0,
    
    /**
     * The previous value of this register is the value CFA+N where CFA is the current CFA value and N is a signed offset.
     */
    PLCRASH_DWARF_CFA_REG_RULE_VAL_OFFSET = 1,
    
    /**
     * The previous value of this register is stored in another register numbered R.
     */
    PLCRASH_DWARF_CFA_REG_RULE_REGISTER = 2,
    
    /**
     * The previous value of this register is located at the address produced by executing the DWARF expression E.
     */
    PLCRASH_DWARF_CFA_REG_RULE_EXPRESSION = 3,
    
    
    /**
     * The previous value of this register is the value produced by executing the DWARF expression E.
     */
    PLCRASH_DWARF_CFA_REG_RULE_VAL_EXPRESSION = 4,

    /**
     * This register has not been modified from the previous frame. (By convention, it is preserved by the callee, but
     * the callee has not modified it.)
     *
     * The register's value may be found in the frame's thread state. For frames other than the first, the
     * register may not have been restored, and thus may be unavailable.
     */
    PLCRASH_DWARF_CFA_REG_RULE_SAME_VALUE = 5,
} plcrash_dwarf_cfa_reg_rule_t;

/**
 * GNU eh_frame pointer state. This is the base state to which DW_EH_PE_t encoded pointer values will be applied.
 */
typedef struct plcrash_async_dwarf_gnueh_ptr_state {
    /**  The pointer size of the target system, in bytes. Must be one of 1, 2, 4, or 8. */
    uint8_t address_size;
    
    /** PC-relative base address to be applied to DW_EH_PE_pcrel offsets, or PL_VM_ADDRESS_INVALID. In the case of FDE
     * entries, this should be the address of the FDE entry itself. */
    uint64_t pc_rel_base;
    
    /**
     * The base address (in-memory) of the loaded debug_frame or eh_frame section, or PL_VM_ADDRESS_INVALID. This is
     * used to calculate the offset of DW_EH_PE_aligned from the start of the frame section.
     *
     * This address should be the actual base address at which the section has been mapped.
     */
    uint64_t frame_section_base;
    
    /**
     * The base VM address of the eh_frame or debug_frame section, or PL_VM_ADDRESS_INVALID. This is used to calculate
     * alignment for DW_EH_PE_aligned-encoded values.
     *
     * This address should be the aligned base VM address at which the section will (or has been loaded) during
     * execution, and will be used to calculate DW_EH_PE_aligned alignment.
     */
    uint64_t frame_section_vm_addr;
    
    /** The base address of the text segment to be applied to DW_EH_PE_textrel offsets, or PL_VM_ADDRESS_INVALID. */
    uint64_t text_base;
    
    /** The base address of the data segment to be applied to DW_EH_PE_datarel offsets, or PL_VM_ADDRESS_INVALID. */
    uint64_t data_base;
    
    /** The base address of the function to be applied to DW_EH_PE_funcrel offsets, or PL_VM_ADDRESS_INVALID. */
    uint64_t func_base;
} plcrash_async_dwarf_gnueh_ptr_state_t;

/** An invalid DWARF GNU EH base address value. */
#define PLCRASH_ASYNC_DWARF_INVALID_BASE_ADDR UINT64_MAX

void plcrash_async_dwarf_gnueh_ptr_state_init (plcrash_async_dwarf_gnueh_ptr_state_t *state, uint8_t address_size);
void plcrash_async_dwarf_gnueh_ptr_state_set_pc_rel_base (plcrash_async_dwarf_gnueh_ptr_state_t *state, uint64_t pc_rel_base);
void plcrash_async_dwarf_gnueh_ptr_state_set_frame_section_base (plcrash_async_dwarf_gnueh_ptr_state_t *state, uint64_t frame_section_base, uint64_t frame_section_vm_addr);
void plcrash_async_dwarf_gnueh_ptr_state_set_text_base (plcrash_async_dwarf_gnueh_ptr_state_t *state, uint64_t text_base);
void plcrash_async_dwarf_gnueh_ptr_state_set_data_base (plcrash_async_dwarf_gnueh_ptr_state_t *state, uint64_t data_base);
void plcrash_async_dwarf_gnueh_ptr_state_set_func_base (plcrash_async_dwarf_gnueh_ptr_state_t *state, uint64_t func_base);

void plcrash_async_dwarf_gnueh_ptr_state_free (plcrash_async_dwarf_gnueh_ptr_state_t *state);

plcrash_error_t plcrash_async_dwarf_read_uleb128 (plcrash_async_mobject_t *mobj, pl_vm_address_t location, pl_vm_off_t offset, uint64_t *result, pl_vm_size_t *size);
plcrash_error_t plcrash_async_dwarf_read_sleb128 (plcrash_async_mobject_t *mobj, pl_vm_address_t location, pl_vm_off_t offset, int64_t *result, pl_vm_size_t *size);

plcrash_error_t plcrash_async_dwarf_read_gnueh_ptr (plcrash_async_mobject_t *mobj,
                                                    const plcrash_async_byteorder_t *byteorder,
                                                    pl_vm_address_t location,
                                                    pl_vm_off_t offset,
                                                    DW_EH_PE_t encoding,
                                                    plcrash_async_dwarf_gnueh_ptr_state_t *ptr_state,
                                                    uint64_t *result,
                                                    uint64_t *size);

plcrash_error_t plcrash_async_dwarf_read_uintmax64 (plcrash_async_mobject_t *mobj,
                                                    const plcrash_async_byteorder_t *byteorder,
                                                    pl_vm_address_t base_addr,
                                                    pl_vm_off_t offset,
                                                    uint8_t data_size,
                                                    uint64_t *dest);


/**
 * @}
 */

#endif /* PLCRASH_ASYNC_DWARF_PRIVATE_H */