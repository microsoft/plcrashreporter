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

#ifndef PLCRASH_ASYNC_DWARF_ENCODING_H
#define PLCRASH_ASYNC_DWARF_ENCODING_H 1

#include "PLCrashAsync.h"
#include "PLCrashAsyncImageList.h"
#include "PLCrashAsyncThread.h"

/**
 * @internal
 * @ingroup plcrash_async_dwarf
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
    PL_DW_EH_PE_indirect = 0x80,
    
    /** No value is present. */
    PL_DW_EH_PE_omit = 0xff,
    
    /** The value is a literal pointer whose size is determined by the architecture. */
    PL_DW_EH_PE_absptr = 0x00,
    
    /** Unsigned value encoded using LEB128 as defined by DWARF Debugging Information Format, Revision 2.0.0. */
    PL_DW_EH_PE_uleb128 = 0x01,
    
    /** Unsigned 16-bit value */
    PL_DW_EH_PE_udata2 = 0x02,
    
    /* Unsigned 32-bit value */
    PL_DW_EH_PE_udata4 = 0x03,
    
    /** Unsigned 64-bit value */
    PL_DW_EH_PE_udata8 = 0x04,
    
    /** Signed value encoded using LEB128 as defined by DWARF Debugging Information Format, Revision 2.0.0. */
    PL_DW_EH_PE_sleb128 = 0x09,
    
    /** Signed 16-bit value */
    PL_DW_EH_PE_sdata2 = 0x0a,
    
    /** Signed 32-bit value */
    PL_DW_EH_PE_sdata4 = 0x0b,
    
    /** Signed 64-bit value */
    PL_DW_EH_PE_sdata8 = 0x0c,
        
    /** Value is relative to the current program counter. */
    PL_DW_EH_PE_pcrel = 0x10,
    
    /** Value is relative to the beginning of the __TEXT section. */
    PL_DW_EH_PE_textrel = 0x20,
    
    /** Value is relative to the beginning of the __DATA section. */
    PL_DW_EH_PE_datarel = 0x30,
    
    /** Value is relative to the beginning of the function. */
    PL_DW_EH_PE_funcrel = 0x40,
    
    /** Value is aligned to an address unit sized boundary. */
    PL_DW_EH_PE_aligned = 0x50,
} PL_DW_EH_PE_t;

/**
 * A DWARF frame reader instance. Performs DWARF eh_frame/debug_frame parsing from a backing memory object.
 */
typedef struct plcrash_async_dwarf_frame_reader {
    /** A memory object containing the DWARF data at the starting address. */
    plcrash_async_mobject_t *mobj;

    /** The byte order of the encoded data. */
    const plcrash_async_byteorder_t *byteorder;
    
    /** True if this is a debug_frame section */
    bool debug_frame;
} plcrash_async_dwarf_frame_reader_t;


/**
 * DWARF Frame Descriptor Entry.
 */
typedef struct plcrash_async_dwarf_fde_info {
    /** The starting address of the FDE entry (not including the initial length field),
     * relative to the eh_frame/debug_frame section base. */
    pl_vm_address_t fde_offset;

    /** The FDE entry length, not including the initial length field. */
    pl_vm_size_t fde_length;

    /** The address of the FDE instructions, relative to the eh_frame/debug_frame section base. */
    pl_vm_address_t fde_instruction_offset;

    /** The start of the IP range covered by this FDE. The address is relative to the image's base address, <em>not</em>
     * the in-memory PC address of a loaded images. */
    pl_vm_address_t pc_start;

    /** The end of the IP range covered by this FDE. The address is relative to the image's base address, <em>not</em>
     * the in-memory PC address of a loaded images.  */
    pl_vm_address_t pc_end;
} plcrash_async_dwarf_fde_info_t;


/**
 * GNU eh_frame pointer state. This is the base state to which PL_DW_EH_PE_t encoded pointer values will be applied.
 */
typedef struct plcrash_async_dwarf_gnueh_ptr_state {
    /**
     * The pointer size of the target system, in bytes; sizes greater than 8 bytes are unsupported.
     */
    pl_vm_address_t address_size;
    
    /** PC-relative base address to be applied to DW_EH_PE_pcrel offsets, or PL_VM_ADDRESS_INVALID. In the case of FDE
     * entries, this should be the address of the FDE entry itself. */
    pl_vm_address_t pc_rel_base;
    
    /** The base address of the text segment to be applied to DW_EH_PE_textrel offsets, or PL_VM_ADDRESS_INVALID. */
    pl_vm_address_t text_base;
    
    /** The base address of the data segment to be applied to DW_EH_PE_datarel offsets, or PL_VM_ADDRESS_INVALID. */
    pl_vm_address_t data_base;
    
    /** The base address of the function to be applied to DW_EH_PE_funcrel offsets, or PL_VM_ADDRESS_INVALID. */
    pl_vm_address_t func_base;
} plcrash_async_dwarf_gnueh_ptr_state_t;


plcrash_error_t plcrash_async_dwarf_frame_reader_init (plcrash_async_dwarf_frame_reader_t *reader,
                                                       plcrash_async_mobject_t *mobj,
                                                       const plcrash_async_byteorder_t *byteorder,
                                                       bool debug_frame);

plcrash_error_t plcrash_async_dwarf_frame_reader_find_fde (plcrash_async_dwarf_frame_reader_t *reader,
                                                           pl_vm_off_t offset,
                                                           pl_vm_address_t pc,
                                                           plcrash_async_dwarf_fde_info_t *fde_info);

void plcrash_async_dwarf_frame_reader_free (plcrash_async_dwarf_frame_reader_t *reader);

void plcrash_async_dwarf_fde_info_free (plcrash_async_dwarf_fde_info_t *fde_info);


void plcrash_async_dwarf_gnueh_ptr_state_init (plcrash_async_dwarf_gnueh_ptr_state_t *state,
                                               pl_vm_address_t address_size,
                                               pl_vm_address_t pc_rel_base,
                                               pl_vm_address_t text_base,
                                               pl_vm_address_t data_base,
                                               pl_vm_address_t func_base);

void plcrash_async_dwarf_gnueh_ptr_state_free (plcrash_async_dwarf_gnueh_ptr_state_t *state);

plcrash_error_t plcrash_async_dwarf_read_uleb128 (plcrash_async_mobject_t *mobj, pl_vm_address_t location, uint64_t *result, pl_vm_size_t *size);
plcrash_error_t plcrash_async_dwarf_read_sleb128 (plcrash_async_mobject_t *mobj, pl_vm_address_t location, int64_t *result, pl_vm_size_t *size);

plcrash_error_t plcrash_async_dwarf_read_gnueh_ptr (plcrash_async_mobject_t *mobj,
                                                    const plcrash_async_byteorder_t *byteorder,
                                                    pl_vm_address_t location,
                                                    PL_DW_EH_PE_t encoding,
                                                    plcrash_async_dwarf_gnueh_ptr_state_t *ptr_state,
                                                    pl_vm_address_t *result,
                                                    pl_vm_size_t *size);


/**
 * @}
 */

#endif /* PLCRASH_ASYNC_DWARF_ENCODING_H */