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

#include "PLCrashAsyncDwarfEncoding.h"

#include <inttypes.h>

/**
 * @internal
 * @ingroup plcrash_async
 * @defgroup plcrash_async_dwarf DWARF
 *
 * Implements async-safe parsing of DWARF encodings.
 * @{
 */

static bool pl_dwarf_read_umax64 (plcrash_async_mobject_t *mobj, const plcrash_async_byteorder_t *byteorder,
                                  pl_vm_address_t base_addr, pl_vm_off_t offset, pl_vm_size_t data_size,
                                  uint64_t *dest);

static bool pl_dwarf_read_vm_address (plcrash_async_mobject_t *mobj, const plcrash_async_byteorder_t *byteorder,
                                      pl_vm_address_t base_addr, pl_vm_off_t offset, bool m64, pl_vm_address_t *dest);

/**
 * Initialize a new DWARF frame reader using the provided memory object. Any resources held by a successfully initialized
 * instance must be freed via plcrash_async_dwarf_frame_reader_free();
 *
 * @param reader The reader instance to initialize.
 * @param mobj The memory object containing frame data (eh_frame or debug_frame) at the start address. This instance must
 * survive for the lifetime of the reader.
 * @param byteoder The byte order of the data referenced by @a mobj.
 * @param debug_frame If true, interpret the DWARF data as a debug_frame section. Otherwise, the
 * frame reader will assume eh_frame data.
 */
plcrash_error_t plcrash_async_dwarf_frame_reader_init (plcrash_async_dwarf_frame_reader_t *reader,
                                                       plcrash_async_mobject_t *mobj,
                                                       const plcrash_async_byteorder_t *byteorder,
                                                       bool debug_frame)
{
    reader->mobj = mobj;
    reader->byteorder = byteorder;
    reader->debug_frame = debug_frame;

    return PLCRASH_ESUCCESS;
}


/**
 * Decode FDE info at target-relative @a address within @a mobj, using the given @a byteorder.
 *
 * @param info The FDE record to be initialized.
 * @param reader The frame reader.
 * @param address The target-relative address containing the FDE data to be decoded. This must include
 * the length field of the FDE.
 */
static plcrash_error_t plcrash_async_dwarf_decode_fde (plcrash_async_dwarf_fde_info_t *info,
                                                       plcrash_async_dwarf_frame_reader_t *reader,
                                                       pl_vm_address_t fde_address)
{
    const plcrash_async_byteorder_t *byteorder = reader->byteorder;
    const pl_vm_address_t base_addr = plcrash_async_mobject_base_address(reader->mobj);

    /* Extract and save the FDE length */
    bool m64;
    pl_vm_size_t length_size;
    {
        uint32_t length32;

        if (plcrash_async_mobject_read_uint32(reader->mobj, byteorder, fde_address, 0x0, &length32) != PLCRASH_ESUCCESS) {
            PLCF_DEBUG("The current FDE entry 0x%" PRIx64 " header lies outside the mapped range", (uint64_t) fde_address);
            return PLCRASH_EINVAL;
        }
        
        if (length32 == UINT32_MAX) {
            pl_dwarf_read_vm_address(reader->mobj, byteorder, fde_address, sizeof(uint32_t), true, &info->fde_length);
            length_size = sizeof(uint64_t) + sizeof(uint32_t);
            m64 = true;
        } else {
            info->fde_length = length32;
            length_size = sizeof(uint32_t);
            m64 = false;
        }
    }
    
    /* Save the FDE offset; this is the FDE address, relative to the mobj base address, not including
     * the FDE initial length. */
    info->fde_offset = (fde_address - base_addr) + length_size;

    /*
     * Calculate the instruction base and offset (eg, the offset to the CIE entry)
     */
    pl_vm_address_t fde_instruction_base;
    pl_vm_off_t fde_instruction_offset;
    {
        pl_vm_address_t raw_offset;
        if (!pl_dwarf_read_vm_address(reader->mobj, byteorder, fde_address, length_size, m64, &raw_offset)) {
            PLCF_DEBUG("FDE instruction offset falls outside the mapped range");
            return PLCRASH_EINVAL;
        }

        if (reader->debug_frame) {
            /* In a .debug_frame, the CIE offset is relative to the start of the section. */
            fde_instruction_base = base_addr;
            fde_instruction_offset = raw_offset;
        } else {
            /* In a .eh_frame, the CIE offset is negative, relative to the current offset of the the FDE. */
            fde_instruction_base = fde_address;
            fde_instruction_offset = -raw_offset;
        }
    }

    /* Apply the offset */
    {
        /* Compute the task-relative address */
        pl_vm_address_t absolute_addr;
        if (!plcrash_async_address_apply_offset(fde_instruction_base, fde_instruction_offset, &absolute_addr)) {
            PLCF_DEBUG("FDE instruction offset overflows the base address");
            return PLCRASH_EINVAL;
        }
        
        /* Convert to a section-relative address */
        if (!plcrash_async_address_apply_offset(absolute_addr, -base_addr, &info->fde_instruction_offset)) {
            PLCF_DEBUG("FDE instruction offset overflows the base address");
            return PLCRASH_EINVAL;
        }
    }

    return PLCRASH_ESUCCESS;
}

/**
 * Locate the frame descriptor entry for @a pc, if available.
 *
 * @param reader The initialized frame reader which will be searched for the entry.
 * @param offset A section-relative offset at which the FDE search will be initiated. This is primarily useful in combination with the compact unwind
 * encoding, in cases where the unwind instructions can not be expressed, and instead a FDE offset is provided by the encoding. Pass an offset of 0
 * to begin searching at the beginning of the unwind data.
 * @param pc The PC value to search for within the frame data. Note that this value must be relative to
 * the target Mach-O image's __TEXT vmaddr.
 * @param fde_info If the FDE is found, PLFRAME_ESUCCESS will be returned and @a fde_info will be initialized with the
 * FDE data. The caller is responsible for freeing the returned FDE record via plcrash_async_dwarf_fde_info_free().
 *
 * @return Returns PLFRAME_ESUCCCESS on success, or one of the remaining error codes if a DWARF parsing error occurs. If
 * the entry can not be found, PLFRAME_ENOTFOUND will be returned.
 */
plcrash_error_t plcrash_async_dwarf_frame_reader_find_fde (plcrash_async_dwarf_frame_reader_t *reader, pl_vm_off_t offset, pl_vm_address_t pc, plcrash_async_dwarf_fde_info_t *fde_info) {
    const plcrash_async_byteorder_t *byteorder = reader->byteorder;
    const pl_vm_address_t base_addr = plcrash_async_mobject_base_address(reader->mobj);
    const pl_vm_address_t end_addr = base_addr + plcrash_async_mobject_length(reader->mobj);

    plcrash_error_t err;

    /* Apply the FDE offset */
    pl_vm_address_t cfi_entry = base_addr;
    if (!plcrash_async_address_apply_offset(base_addr, offset, &cfi_entry)) {
        PLCF_DEBUG("FDE offset hint overflows the mobject's base address");
        return PLCRASH_EINVAL;
    }

    if (cfi_entry >= end_addr) {
        PLCF_DEBUG("FDE base address + offset falls outside the mapped range");
        return PLCRASH_EINVAL;
    }

    /* Iterate over table entries */
    while (cfi_entry < end_addr) {
        /* Fetch the entry length (and determine wether it's 64-bit or 32-bit) */
        uint64_t length;
        pl_vm_size_t length_size;
        bool m64;

        {
            uint32_t *length32 = plcrash_async_mobject_remap_address(reader->mobj, cfi_entry, 0x0, sizeof(uint32_t));
            if (length32 == NULL) {
                PLCF_DEBUG("The current CFI entry 0x%" PRIx64 " header lies outside the mapped range", (uint64_t) cfi_entry);
                return PLCRASH_EINVAL;
            }
            
            if (byteorder->swap32(*length32) == UINT32_MAX) {
                uint64_t *length64 = plcrash_async_mobject_remap_address(reader->mobj, cfi_entry, sizeof(uint32_t), sizeof(uint64_t));
                if (length64 == NULL) {
                    PLCF_DEBUG("The current CFI entry 0x%" PRIx64 " header lies outside the mapped range", (uint64_t) cfi_entry);
                    return PLCRASH_EINVAL;
                }

                length = byteorder->swap64(*length64);
                length_size = sizeof(uint64_t) + sizeof(uint32_t);
                m64 = true;
            } else {
                length = byteorder->swap32(*length32);
                length_size = sizeof(uint32_t);
                m64 = false;
            }
        }

        /*
         * APPLE EXTENSION
         * Check for end marker, as per Apple's libunwind-35.1. It's unclear if this is defined by the DWARF 3 or 4 specifications; I could not
         * find a reference to it.
         
         * Section 7.2.2 defines 0xfffffff0 - 0xffffffff as being reserved for extensions to the length
         * field relative to the DWARF 2 standard. There is no explicit reference to the use of an 0 value.
         *
         * In section 7.2.1, the value of 0 is defined as being reserved as an error value in the encodings for
         * "attribute names, attribute forms, base type encodings, location operations, languages, line number program
         * opcodes, macro information entries and tag names to represent an error condition or unknown value."
         *
         * Section 7.2.2 doesn't justify the usage of 0x0 as a termination marker, but given that Apple's code relies on it,
         * we will also do so here.
         */
        if (length == 0x0)
            return PLCRASH_ENOTFOUND;
        
        /* Calculate the next entry address; the length_size addition is known-safe, as we were able to successfully read the length from *cfi_entry */
        pl_vm_address_t next_cfi_entry;
        if (!plcrash_async_address_apply_offset(cfi_entry+length_size, length, &next_cfi_entry)) {
            PLCF_DEBUG("Entry length size overflows the CFI address");
            return PLCRASH_EINVAL;
        }
        
        /* Fetch the entry id */
        pl_vm_address_t cie_id;
        if (!pl_dwarf_read_vm_address(reader->mobj, byteorder, cfi_entry, length_size, m64, &cie_id)) {
            PLCF_DEBUG("The current CFI entry 0x%" PRIx64 " cie_id lies outside the mapped range", (uint64_t) cfi_entry);
            return PLCRASH_EINVAL;
        }

        /* Check for (and skip) CIE entries. */
        {
            bool is_cie = false;
    
            /* debug_frame uses UINT?_MAX to denote CIE entries. */
            if (reader->debug_frame && ((m64 && cie_id == UINT64_MAX) || (!m64 && cie_id == UINT32_MAX)))
                is_cie = true;
            
            /* eh_frame uses a type of 0x0 to denote CIE entries. */
            if (!reader->debug_frame && cie_id == 0x0)
                is_cie = true;

            /* If not a FDE, skip */
            if (is_cie) {
                /* Not a FDE -- skip */
                cfi_entry = next_cfi_entry;
                continue;
            }
        }

        /* Decode the FDE */
        err = plcrash_async_dwarf_decode_fde(fde_info, reader, cfi_entry);
        if (err != PLCRASH_ESUCCESS)
            return err;

        // TODO
        return PLCRASH_ESUCCESS;

        /* Skip to the next entry */
        cfi_entry = next_cfi_entry;
    }

    // TODO
    return PLCRASH_ESUCCESS;
}



/**
 * Free all resources associated with @a reader.
 */
void plcrash_async_dwarf_frame_reader_free (plcrash_async_dwarf_frame_reader_t *reader) {
    // noop
}

/**
 * Free all resources associated with @a fde_info.
 */
void plcrash_async_dwarf_fde_info_free (plcrash_async_dwarf_fde_info_t *fde_info) {
    // noop
}


/**
 * Initialize GNU eh_frame pointer @a state. This is the base state to which PL_DW_EH_PE_t encoded pointer values will be applied.
 *
 * @param state The state value to be initialized.
 * @param address_size The pointer size of the target system, in bytes; must be one of 1, 2, 4, or 8.
 * @param frame_section_base The base address (in-memory) of the loaded debug_frame or eh_frame section, or PL_VM_ADDRESS_INVALID. This is
 * used to calculate the offset of DW_EH_PE_aligned from the start of the frame section. This address should be the
 * actual base address at which the section has been mapped.
 *
 * @param frame_section_vm_addr The base VM address of the eh_frame or debug_frame section, or PL_VM_ADDRESS_INVALID.
 * This is used to calculate alignment for DW_EH_PE_aligned-encoded values. This address should be the aligned base VM
 * address at which the section will (or has been loaded) during execution, and will be used to calculate
 * PL_DW_EH_PE_aligned alignment.
 *
 * @param pc_rel_base PC-relative base address to be applied to DW_EH_PE_pcrel offsets, or PL_VM_ADDRESS_INVALID. In the case of FDE
 * entries, this should be the address of the FDE entry itself.
 * @param text_base The base address of the text segment to be applied to DW_EH_PE_textrel offsets, or PL_VM_ADDRESS_INVALID.
 * @param data_base The base address of the data segment to be applied to DW_EH_PE_datarel offsets, or PL_VM_ADDRESS_INVALID.
 * @param func_base The base address of the function to be applied to DW_EH_PE_funcrel offsets, or PL_VM_ADDRESS_INVALID.
 *
 * Any resources held by a successfully initialized instance must be freed via plcrash_async_dwarf_gnueh_ptr_state_free();
 */
void plcrash_async_dwarf_gnueh_ptr_state_init (plcrash_async_dwarf_gnueh_ptr_state_t *state,
                                               pl_vm_address_t address_size,
                                               pl_vm_address_t frame_section_base,
                                               pl_vm_address_t frame_section_vm_addr,
                                               pl_vm_address_t pc_rel_base,
                                               pl_vm_address_t text_base,
                                               pl_vm_address_t data_base,
                                               pl_vm_address_t func_base)
{
    PLCF_ASSERT(address_size == 1 || address_size == 2 || address_size == 4 || address_size == 8);

    state->address_size = address_size;
    state->frame_section_base = frame_section_base;
    state->frame_section_vm_addr = frame_section_vm_addr;
    state->pc_rel_base = pc_rel_base;
    state->text_base = text_base;
    state->data_base = data_base;
    state->func_base = func_base;
}

/**
 * Free all resources associated with @a state.
 */
void plcrash_async_dwarf_gnueh_ptr_state_free (plcrash_async_dwarf_gnueh_ptr_state_t *state) {
    // noop
}


/**
 * Read a DWARF encoded pointer value from @a location within @a mobj. The encoding format is defined in
 * the Linux Standard Base Core Specification 4.1, section 10.5, DWARF Extensions.
 *
 * @param mobj The memory object from which the pointer data (including TEXT/DATA-relative values) will be read. This
 * should map the full binary that may be read; the pointer value may reference data that is relative to the binary
 * sections, depending on the base addresses supplied via @a state.
 * @param byteoder The byte order of the data referenced by @a mobj.
 * @param location A task-relative location within @a mobj.
 * @param encoding The encoding method to be used to decode the target pointer
 * @param state The base GNU eh_frame pointer state to which the encoded pointer value will be applied. If a value
 * is read that is relative to a @state-supplied base address of PL_VM_ADDRESS_INVALID, PLCRASH_ENOTSUP will be returned.
 * @param result On success, the pointer value.
 * @param size On success, will be set to the total size of the pointer data read at @a location, in bytes.
 */
plcrash_error_t plcrash_async_dwarf_read_gnueh_ptr (plcrash_async_mobject_t *mobj,
                                                    const plcrash_async_byteorder_t *byteorder,
                                                    pl_vm_address_t location,
                                                    PL_DW_EH_PE_t encoding,
                                                    plcrash_async_dwarf_gnueh_ptr_state_t *state,
                                                    pl_vm_address_t *result,
                                                    pl_vm_size_t *size)
{
    plcrash_error_t err;

    /* Skip DW_EH_pe_omit -- as per LSB 4.1.0, this signifies that no value is present */
    if (encoding == PL_DW_EH_PE_omit) {
        PLCF_DEBUG("Skipping decoding of DW_EH_PE_omit pointer");
        return PLCRASH_ENOTFOUND;
    }
    
    /* Initialize the output size; we apply offsets to this size to allow for aligning the
     * address prior to reading the pointer data, etc. */
    *size = 0;

    /* Calculate the base address; bits 5-8 are used to specify the relative offset type */
    pl_vm_address_t base;
    switch (encoding & 0x70) {
        case PL_DW_EH_PE_pcrel:
            if (state->pc_rel_base == PL_VM_ADDRESS_INVALID) {
                PLCF_DEBUG("Cannot decode DW_EH_PE_pcrel value with PL_VM_ADDRESS_INVALID pc_rel_base");
                return PLCRASH_ENOTSUP;
            }
    
            base = state->pc_rel_base;
            break;
            
        case PL_DW_EH_PE_absptr:
            /* No flags are set */
            base = 0x0;
            break;
            
        case PL_DW_EH_PE_textrel:
            if (state->text_base == PL_VM_ADDRESS_INVALID) {
                PLCF_DEBUG("Cannot decode DW_EH_PE_textrel value with PL_VM_ADDRESS_INVALID text_addr");
                return PLCRASH_ENOTSUP;
            }
            base = state->text_base;
            break;
            
        case PL_DW_EH_PE_datarel:
            if (state->data_base == PL_VM_ADDRESS_INVALID) {
                PLCF_DEBUG("Cannot decode DW_EH_PE_datarel value with PL_VM_ADDRESS_INVALID data_base");
                return PLCRASH_ENOTSUP;
            }
            base = state->data_base;
            break;

        case PL_DW_EH_PE_funcrel:
            if (state->func_base == PL_VM_ADDRESS_INVALID) {
                PLCF_DEBUG("Cannot decode DW_EH_PE_funcrel value with PL_VM_ADDRESS_INVALID func_base");
                return PLCRASH_ENOTSUP;
            }
            
            base = state->func_base;
            break;
            
        case PL_DW_EH_PE_aligned: {
            /* Verify availability of required base addresses */
            if (state->frame_section_vm_addr == PL_VM_ADDRESS_INVALID) {
                PLCF_DEBUG("Cannot decode DW_EH_PE_aligned value with PL_VM_ADDRESS_INVALID frame_section_vm_addr");
                return PLCRASH_ENOTSUP;
            } else if (state->frame_section_base == PL_VM_ADDRESS_INVALID) {
                PLCF_DEBUG("Cannot decode DW_EH_PE_aligned value with PL_VM_ADDRESS_INVALID frame_section_base");
                return PLCRASH_ENOTSUP;
            }
            
            /* Compute the offset+alignment relative to the section base */
            PLCF_ASSERT(location >= state->frame_section_base);
            pl_vm_address_t offset = location - state->frame_section_base;
            
            /* Apply to the VM load address for the section. */
            pl_vm_address_t vm_addr = state->frame_section_vm_addr + offset;
            pl_vm_address_t vm_aligned = (vm_addr + (state->address_size-1)) & ~(state->address_size);
            
            /* Apply the new offset to the actual load address */
            location += (vm_aligned - vm_addr);

            /* Set the base size to the number of bytes skipped */
            base = 0x0;
            *size = (vm_aligned - vm_addr);
            break;
        }

        default:
            PLCF_DEBUG("Unsupported pointer base encoding of 0x%x", encoding);
            return PLCRASH_ENOTSUP;
    }

    /*
     * Decode and return the pointer value [+ offset].
     *
     * TODO: This code permits overflow to occur under the assumption that the failure will be caught
     * when safely dereferencing the resulting address. This should only occur when either bad data is presented,
     * or due to an implementation flaw in this code path -- in those cases, it would be preferable to
     * detect overflow early.
     */
    switch (encoding & 0x0F) {
        case PL_DW_EH_PE_absptr: {
            uint64_t u64;

            if (!pl_dwarf_read_umax64(mobj, byteorder, location, 0, state->address_size, &u64)) {
                PLCF_DEBUG("Failed to read value at 0x%" PRIx64, (uint64_t) location);
                return PLCRASH_EINVAL;
            }
            
            *result = u64 + base;
            *size += state->address_size;
            break;
        }

        case PL_DW_EH_PE_uleb128: {
            uint64_t ulebv;
            pl_vm_address_t uleb_size;

            err = plcrash_async_dwarf_read_uleb128(mobj, location, &ulebv, &uleb_size);
            
            /* There's no garuantee that PL_VM_ADDRESS_MAX >= UINT64_MAX on all platforms */
            if (ulebv > PL_VM_ADDRESS_MAX) {
                PLCF_DEBUG("ULEB128 value exceeds PL_VM_ADDRESS_MAX");
                return PLCRASH_ENOTSUP;
            }
            
            *result = ulebv + base;
            *size += uleb_size;
            break;
        }

        case PL_DW_EH_PE_udata2: {
            uint16_t udata2;
            if ((err = plcrash_async_mobject_read_uint16(mobj, byteorder, location, 0, &udata2)) != PLCRASH_ESUCCESS)
                return err;

            *result = udata2 + base;
            *size += 2;
            break;
        }
            
        case PL_DW_EH_PE_udata4: {
            uint32_t udata4;
            if ((err = plcrash_async_mobject_read_uint32(mobj, byteorder, location, 0, &udata4)) != PLCRASH_ESUCCESS)
                return err;
            
            *result = udata4 + base;
            *size += 4;
            break;
        }
            
        case PL_DW_EH_PE_udata8: {
            uint64_t udata8;
            if ((err = plcrash_async_mobject_read_uint64(mobj, byteorder, location, 0, &udata8)) != PLCRASH_ESUCCESS)
                return err;

            *result = udata8 + base;
            *size += 8;
            break;
        }

        case PL_DW_EH_PE_sleb128: {
            int64_t slebv;
            pl_vm_size_t sleb_size;

            err = plcrash_async_dwarf_read_sleb128(mobj, location, &slebv, &sleb_size);
            
            /* There's no garuantee that PL_VM_ADDRESS_MAX >= INT64_MAX on all platforms */
            if (slebv > PL_VM_OFF_MAX || slebv < PL_VM_OFF_MIN) {
                PLCF_DEBUG("SLEB128 value exceeds PL_VM_OFF_MIN/PL_VM_OFF_MAX");
                return PLCRASH_ENOTSUP;
            }
            
            *result = slebv + base;
            *size += sleb_size;
            break;
        }
            
        case PL_DW_EH_PE_sdata2: {
            int16_t sdata2;
            if ((err = plcrash_async_mobject_read_uint16(mobj, byteorder, location, 0, (uint16_t *) &sdata2)) != PLCRASH_ESUCCESS)
                return err;

            *result = sdata2 + base;
            *size += 2;
            break;
        }
            
        case PL_DW_EH_PE_sdata4: {
            int32_t sdata4;
            if ((err = plcrash_async_mobject_read_uint32(mobj, byteorder, location, 0, (uint32_t *) &sdata4)) != PLCRASH_ESUCCESS)
                return err;
            
            *result = sdata4 + base;
            *size += 4;
            break;
        }
            
        case PL_DW_EH_PE_sdata8: {
            int64_t sdata8;
            if ((err = plcrash_async_mobject_read_uint64(mobj, byteorder, location, 0, (uint64_t *) &sdata8)) != PLCRASH_ESUCCESS)
                return err;
    
            *result = sdata8 + base;
            *size += 8;
            break;
        }
            
        default:
            PLCF_DEBUG("Unknown pointer encoding of type 0x%x", encoding);
            return PLCRASH_ENOTSUP;
    }

    /* Handle indirection; the target value may only be an absptr; there is no way to define an
     * encoding for the indirected target. */
    if (encoding & PL_DW_EH_PE_indirect) {
        /* The size of the target doesn't matter; the caller only needs to know how many bytes were read from
         * @a location */
        pl_vm_size_t target_size;
        return plcrash_async_dwarf_read_gnueh_ptr(mobj, byteorder, *result, PL_DW_EH_PE_absptr, state, result, &target_size);
    }

    return PLCRASH_ESUCCESS;
}

/**
 * Read a ULEB128 value from @a location within @a mobj.
 *
 * @param mobj The memory object from which the LEB128 data will be read.
 * @param location A task-relative location within @a mobj.
 * @param result On success, the ULEB128 value.
 * @param size On success, will be set to the total size of the decoded LEB128 value at @a location, in bytes.
 */
plcrash_error_t plcrash_async_dwarf_read_uleb128 (plcrash_async_mobject_t *mobj, pl_vm_address_t location, uint64_t *result, pl_vm_size_t *size) {
    unsigned int shift = 0;
    pl_vm_off_t offset = 0;
    *result = 0;
    
    uint8_t *p;
    while ((p = plcrash_async_mobject_remap_address(mobj, location, offset, 1)) != NULL) {
        /* LEB128 uses 7 bits for the number, the final bit to signal completion */
        uint8_t byte = *p;
        *result |= ((uint64_t) (byte & 0x7f)) << shift;
        shift += 7;

        /* This is used to track length, so we must set it before
         * potentially terminating the loop below */
        offset++;
        
        /* Check for terminating bit */
        if ((byte & 0x80) == 0)
            break;
        
        /* Check for a ULEB128 larger than 64-bits */
        if (shift >= 64) {
            PLCF_DEBUG("ULEB128 is larger than the maximum supported size of 64 bits");
            return PLCRASH_ENOTSUP;
        }        
    }

    if (p == NULL) {
        PLCF_DEBUG("ULEB128 value did not terminate within mapped memory range");
        return PLCRASH_EINVAL;
    }

    *size = offset;
    return PLCRASH_ESUCCESS;
}

/**
 * Read a SLEB128 value from @a location within @a mobj.
 *
 * @param mobj The memory object from which the LEB128 data will be read.
 * @param location A task-relative location within @a mobj.
 * @param result On success, the ULEB128 value.
 * @param size On success, will be set to the total size of the decoded LEB128 value, in bytes.
 */
plcrash_error_t plcrash_async_dwarf_read_sleb128 (plcrash_async_mobject_t *mobj, pl_vm_address_t location, int64_t *result, pl_vm_size_t *size) {
    unsigned int shift = 0;
    pl_vm_off_t offset = 0;
    *result = 0;
    
    uint8_t *p;
    while ((p = plcrash_async_mobject_remap_address(mobj, location, offset, 1)) != NULL) {
        /* LEB128 uses 7 bits for the number, the final bit to signal completion */
        uint8_t byte = *p;
        *result |= ((uint64_t) (byte & 0x7f)) << shift;
        shift += 7;
        
        /* This is used to track length, so we must set it before
         * potentially terminating the loop below */
        offset++;
        
        /* Check for terminating bit */
        if ((byte & 0x80) == 0)
            break;
        
        /* Check for a ULEB128 larger than 64-bits */
        if (shift >= 64) {
            PLCF_DEBUG("ULEB128 is larger than the maximum supported size of 64 bits");
            return PLCRASH_ENOTSUP;
        }
    }
    
    if (p == NULL) {
        PLCF_DEBUG("ULEB128 value did not terminate within mapped memory range");
        return PLCRASH_EINVAL;
    }
    
    /* Sign bit is 2nd high order bit */
    if (shift < 64 && (*p & 0x40))
        *result |= -(1ULL << shift);

    *size = offset;
    return PLCRASH_ESUCCESS;
}

/**
 * @internal
 *
 * Read a value that is either 1, 2, 4, or 8 bytes in size. Returns true on success, false on failure.
 *
 * @param mobj Memory object from which to read the value.
 * @param byteorder Byte order of the target value.
 * @param base_addr The base address (within @a mobj's address space) from which to perform the read.
 * @param offset An offset to be applied to base_addr.
 * @param data_size The size of the value to be read. If an unsupported size is supplied, false will be returned.
 * @param dest The destination value.
 */
static bool pl_dwarf_read_umax64 (plcrash_async_mobject_t *mobj, const plcrash_async_byteorder_t *byteorder,
                                  pl_vm_address_t base_addr, pl_vm_off_t offset, pl_vm_size_t data_size,
                                  uint64_t *dest)
{
    union {
        uint8_t u8;
        uint16_t u16;
        uint32_t u32;
        uint64_t u64;
    } *data;
    
    data = plcrash_async_mobject_remap_address(mobj, base_addr, offset, data_size);
    if (data == NULL)
        return false;
    
    switch (data_size) {
        case 1:
            *dest = data->u8;
            break;
            
        case 2:
            *dest = byteorder->swap16(data->u16);
            break;

        case 4:
            *dest = byteorder->swap32(data->u32);
            break;

        case 8:
            *dest = byteorder->swap64(data->u64);
            break;

        default:
            PLCF_DEBUG("Unhandled data width %" PRIu64, (uint64_t) data_size);
            return false;
    }
    
    return true;
}


/**
 * @internal
 *
 * Read a VM address value.
 *
 * @param mobj Memory object from which to read the value.
 * @param byteorder Byte order of the target value.
 * @param base_addr The base address (within @a mobj's address space) from which to perform the read.
 * @param offset An offset to be applied to base_addr.
 * @param m64 If true, a 64-bit value will be read. If false, a 32-bit value will be read.
 * @param dest The destination value.
 */
static bool pl_dwarf_read_vm_address (plcrash_async_mobject_t *mobj, const plcrash_async_byteorder_t *byteorder,
                                      pl_vm_address_t base_addr, pl_vm_off_t offset, bool m64, pl_vm_address_t *dest)
{
    if (m64) {
        uint64_t r64;
        if (plcrash_async_mobject_read_uint64(mobj, byteorder, base_addr, offset, &r64) != PLCRASH_ESUCCESS)
            return false;
        
        *dest = r64;
        return true;
    } else {
        uint32_t r32;
        if (plcrash_async_mobject_read_uint32(mobj, byteorder, base_addr, offset, &r32) != PLCRASH_ESUCCESS)
            return false;
        *dest = r32;
        return true;
    }
}

/**
 * @}
 */