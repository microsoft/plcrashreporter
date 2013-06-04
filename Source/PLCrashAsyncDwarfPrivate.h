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
 * @ingroup plcrash_async_dwarf_private
 * @{
 */

/**
 * Exception handling pointer encoding constants, as defined by the LSB Specification:
 * http://refspecs.linuxfoundation.org/LSB_4.1.0/LSB-Core-generic/LSB-Core-generic/dwarfext.html
 *
 * The upper 4 bits indicate how the value is to be applied. The lower 4 bits indicate the format of the data.
 */
typedef enum PL_DW_EH_PE {
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

/**
 * GNU eh_frame pointer state. This is the base state to which DW_EH_PE_t encoded pointer values will be applied.
 */
typedef struct plcrash_async_dwarf_gnueh_ptr_state {
    /**
     * The pointer size of the target system, in bytes; sizes greater than 8 bytes are unsupported.
     */
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

/**
 * DWARF Common Information Entry.
 */
typedef struct plcrash_async_dwarf_cie_info {
    /**
     * The task-relative address of the CIE record (not including the initial length field),
     * relative to the eh_frame/debug_frame section base.
     */
    uint64_t cie_offset;

    /** The CIE record length, not including the initial length field. */
    uint64_t cie_length;
    
    /**
     * The CIE identifier. This will be either 4 or 8 bytes, depending on the decoded DWARF
     * format.
     *
     * @par GCC .eh_frame.
     * For GCC3 eh_frame sections, this value will always be 0.
     *
     * @par DWARF
     * For DWARF debug_frame sections, this value will always be UINT32_MAX or UINT64_MAX for
     * the DWARF 32-bit and 64-bit formats, respectively.
     */
    uint64_t cie_id;
    
    /**
     * The CIE version. Supported version numbers:
     * - GCC3 .eh_frame: 1
     * - DWARF3 debug_frame: 3
     * - DWARF4 debug_frame: 4
     */
    uint8_t cie_version;
    
    /**
     * The size in bytes of an address (or the offset portion of an address for segmented addressing) on
     * the target system. This value will only be present in DWARF4 CIE records; on non-DWARF4 records,
     * the value will be initialized to zero.
     *
     * Defined in the DWARF4 standard, Section 7.20.
     */
    uint8_t address_size;

    /**
     * The size in bytes of a segment selector on the target system, or 0. This value will
     * only be present in DWARF4 CIE records; on non-DWARF4 records, the value will be initialized
     * to zero.
     *
     * Defined in the DWARF4 standard, Section 7.20.
     */
    uint8_t segment_size;
    
    /**
     * Code alignment factor. A constant that is factored out of all advance location instructions; see DWARF4 Section 6.4.2.1.
     */
    uint64_t code_alignment_factor;

    /** Data alignment factor. A constant that is factored out of certain offset instructions; see DWARF4 Section 6.4.2.1. */
    uint64_t data_alignment_factor;

    /** Return address register. A constant that constant that indicates which column in the rule table represents the return
     * address of the function. Note that this column might not correspond to an actual machine register. */
    uint64_t return_address_register;
#if 0


    /** The address of the CIE initial instructions, relative to the eh_frame/debug_frame section base. */
    pl_vm_address_t cie_initial_instruction_offset;
    
    /** The pointer encoding format. */
    DW_EH_PE_t ptr_encoding;

    pint_t		cieInstructions;
    uint8_t		pointerEncoding;
    uint8_t		lsdaEncoding;
    uint8_t		personalityEncoding;
    uint8_t		personalityOffsetInCIE;
    pint_t		personality;
    int			codeAlignFactor;
    int			dataAlignFactor;
    bool		isSignalFrame;
    bool		fdesHaveAugmentationData;

    /** The CIE's starting and ending offset within the debug info section, minus length. */
    uintptr_t cieStart, cieEnd;
    
    /** The CIE's length, not counting the length field. This is the parsed
     * length value, not the raw length bytes. */
    size_t length;
    
    /** The CIE ID. Not particularly useful, included for completeness. */
    uint64_t cieID;
    
    /** The CIE version. Either 1 (GCC) or 3 (DWARF 2 spec). */
    uint8_t version;
    
    /** Flag whether FDEs based on this CIE have augmentation data. */
    bool hasAugmentationData;
    
    /** Augmentation data size, if hasAugmentationData is true. */
    uint64_t augmentationDataSize;
    
    /** Personality routine pointer, if any. */
    uintptr_t personalityRoutine;
    
    /** Language-Specific Data Area encoding. */
    uint8_t lsdaEncoding;
    
    /** Pointer encoding format for FDEs. */
    uint8_t pointerEncoding;
    
    /** Signal frame flag. */
    bool isSignalFrame;
    
    /** Code alignment factor. Factored out of advance location instructions. */
    uint64_t codeAlignmentFactor;
    
    /** Data alignment factor. Factored out of offset instructions. */
    int64_t dataAlignmentFactor;
    
    /** Return address register column. */
    uint8_t returnAddressColumn;
    
    /** The raw offset in the debug info section to the initial instructions.
     * This is not a set of parsed instructions because that is only done at
     * DWARF step time. This structure is meant to reduce memory allocation,
     * not encapsulate all available information. */
    uintptr_t initialInstructionsStart;
#endif
} plcrash_async_dwarf_cie_info_t;

/** An invalid DWARF GNU EH base address value. */
#define PLCRASH_ASYNC_DWARF_INVALID_BASE_ADDR UINT64_MAX

plcrash_error_t plcrash_async_dwarf_cie_info_init (plcrash_async_dwarf_cie_info_t *info,
                                                   plcrash_async_mobject_t *mobj,
                                                   const plcrash_async_byteorder_t *byteorder,
                                                   pl_vm_address_t address);

void plcrash_async_dwarf_cie_info_free (plcrash_async_dwarf_cie_info_t *info);

void plcrash_async_dwarf_gnueh_ptr_state_init (plcrash_async_dwarf_gnueh_ptr_state_t *state,
                                               uint8_t address_size,
                                               uint64_t frame_section_base,
                                               uint64_t frame_section_vm_addr,
                                               uint64_t pc_rel_base,
                                               uint64_t text_base,
                                               uint64_t data_base,
                                               uint64_t func_base);

void plcrash_async_dwarf_gnueh_ptr_state_free (plcrash_async_dwarf_gnueh_ptr_state_t *state);

plcrash_error_t plcrash_async_dwarf_read_uleb128 (plcrash_async_mobject_t *mobj, pl_vm_address_t location, pl_vm_off_t offset, uint64_t *result, pl_vm_size_t *size);
plcrash_error_t plcrash_async_dwarf_read_sleb128 (plcrash_async_mobject_t *mobj, pl_vm_address_t location, pl_vm_off_t offset, int64_t *result, pl_vm_size_t *size);

plcrash_error_t plcrash_async_dwarf_read_gnueh_ptr (plcrash_async_mobject_t *mobj,
                                                    const plcrash_async_byteorder_t *byteorder,
                                                    pl_vm_address_t location,
                                                    DW_EH_PE_t encoding,
                                                    plcrash_async_dwarf_gnueh_ptr_state_t *ptr_state,
                                                    uint64_t *result,
                                                    uint64_t *size);


/**
 * @}
 */

#endif /* PLCRASH_ASYNC_DWARF_PRIVATE_H */