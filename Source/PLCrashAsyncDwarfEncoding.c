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

static bool pl_dwarf_read_u16 (plcrash_async_mobject_t *mobj, const plcrash_async_byteorder_t *byteorder,
                               pl_vm_address_t base_addr, pl_vm_off_t offset, uint16_t *dest);

static bool pl_dwarf_read_u32 (plcrash_async_mobject_t *mobj, const plcrash_async_byteorder_t *byteorder,
                               pl_vm_address_t base_addr, pl_vm_off_t offset, uint32_t *dest);

static bool pl_dwarf_read_u64 (plcrash_async_mobject_t *mobj, const plcrash_async_byteorder_t *byteorder,
                               pl_vm_address_t base_addr, pl_vm_off_t offset, uint64_t *dest);

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
 * @param length The length of the FDE element (not including the entry's variable 'initial length' field)
 * @param m64 If true, the FDE will be parsed as a 64-bit entry. If false, will be parsed as a 32-bit entry.
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

        if (!pl_dwarf_read_u32(reader->mobj, byteorder, fde_address, 0x0, &length32)) {
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
 * Read a DWARF encoded pointer value from @a location within @a mobj. The encoding format is defined in
 * the Linux Standard Base Core Specification 4.1, section 10.5, DWARF Extensions.
 *
 * @param mobj The memory object from which the pointer data will be read.
 * @param location A task-relative location within @a mobj.
 * @param result On success, the pointer value.
 * @param size On success, will be set to the total size of the pointer data read at @a location, in bytes.
 */
plcrash_error_t plcrash_async_dwarf_read_gnueh_ptr (plcrash_async_mobject_t *mobj, const plcrash_async_byteorder_t *byteorder,
                                                      pl_vm_address_t location, PL_DW_EH_PE_t encoding,
                                                      pl_vm_address_t *result, pl_vm_size_t *size)
{
    plcrash_error_t err;

    /* Skip DW_EH_pe_omit -- as per LSB 4.1.0, this signifies that no value is present */
    if (encoding == PL_DW_EH_PE_omit) {
        PLCF_DEBUG("Skipping decoding of DW_EH_PE_omit pointer");
        return PLCRASH_ENOTFOUND;
    }

    /* Calculate the base address. Currently, only pcrel and absptr are supported; this matches
     * the behavior of Darwin's libunwind */
    pl_vm_address_t base;
    switch (encoding & 0x70) {
        case PL_DW_EH_PE_pcrel:
            base = location;
            break;
            
        case PL_DW_EH_PE_absptr:
            base = 0x0;
            break;
            
        case PL_DW_EH_PE_textrel:
        case PL_DW_EH_PE_datarel:
        case PL_DW_EH_PE_funcrel:
        case PL_DW_EH_PE_aligned:
            // TODO!

        default:
            PLCF_DEBUG("Unsupported pointer base encoding of 0x%x", encoding);
            return PLCRASH_ENOTSUP;
    }

    /* Read and apply the pointer value */
    switch (encoding & 0x0F) {
        case PL_DW_EH_PE_absptr:
            // TODO - we need to know the native pointer size of the target.
            return PLCRASH_ENOTSUP;

        case PL_DW_EH_PE_uleb128: {
            uint64_t ulebv;            
            err = plcrash_async_dwarf_read_uleb128(mobj, location, &ulebv, size);
            
            /* There's no garuantee that PL_VM_ADDRESS_MAX >= UINT64_MAX on all platforms */
            if (ulebv > PL_VM_ADDRESS_MAX) {
                PLCF_DEBUG("ULEB128 value exceeds PL_VM_ADDRESS_MAX");
                return PLCRASH_ENOTSUP;
            }
            
            *result = ulebv + base;
            return PLCRASH_ESUCCESS;
        }

        case PL_DW_EH_PE_udata2: {
            uint16_t udata2;
            if (!pl_dwarf_read_u16(mobj, byteorder, location, 0, &udata2))
                return PLCRASH_EINVAL;

            *result = udata2 + base;
            *size = 2;
            return PLCRASH_ESUCCESS;
        }
            
        case PL_DW_EH_PE_udata4: {
            uint32_t udata4;
            if (!pl_dwarf_read_u32(mobj, byteorder, location, 0, &udata4))
                return PLCRASH_EINVAL;
            
            *result = udata4 + base;
            *size = 4;
            return PLCRASH_ESUCCESS;
        }
            
        case PL_DW_EH_PE_udata8: {
            uint64_t udata8;
            if (!pl_dwarf_read_u64(mobj, byteorder, location, 0, &udata8))
                return PLCRASH_EINVAL;
            
            *result = udata8 + base;
            *size = 8;
            return PLCRASH_ESUCCESS;
        }

        case PL_DW_EH_PE_sleb128: {
            int64_t slebv;
            err = plcrash_async_dwarf_read_sleb128(mobj, location, &slebv, size);
            
            /* There's no garuantee that PL_VM_ADDRESS_MAX >= INT64_MAX on all platforms */
            if (slebv > PL_VM_OFF_MAX || slebv < PL_VM_OFF_MIN) {
                PLCF_DEBUG("SLEB128 value exceeds PL_VM_OFF_MIN/PL_VM_OFF_MAX");
                return PLCRASH_ENOTSUP;
            }
            
            *result = slebv + base;
            return PLCRASH_ESUCCESS;
        }
            
        case PL_DW_EH_PE_sdata2: {
            int16_t sdata2;
            if (!pl_dwarf_read_u16(mobj, byteorder, location, 0, (uint16_t *) &sdata2))
                return PLCRASH_EINVAL;
            
            *result = sdata2 + base;
            *size = 2;
            return PLCRASH_ESUCCESS;
        }
            
        case PL_DW_EH_PE_sdata4: {
            int32_t sdata4;
            if (!pl_dwarf_read_u32(mobj, byteorder, location, 0, (uint32_t *) &sdata4))
                return PLCRASH_EINVAL;
            
            *result = sdata4 + base;
            *size = 4;
            return PLCRASH_ESUCCESS;
        }
            
        case PL_DW_EH_PE_sdata8: {
            int64_t sdata8;
            if (!pl_dwarf_read_u64(mobj, byteorder, location, 0, (uint64_t *) &sdata8))
                return PLCRASH_EINVAL;
            
            *result = sdata8 + base;
            *size = 8;
            return PLCRASH_ESUCCESS;
        }
            
        default:
            PLCF_DEBUG("Unknown pointer encoding of type 0x%x", encoding);
            return PLCRASH_ENOTSUP;
    }

    /* Unreachable */
    return PLCRASH_EINTERNAL;
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
 * Read a 16-bit value.
 *
 * @param mobj Memory object from which to read the value.
 * @param byteorder Byte order of the target value.
 * @param base_addr The base address (within @a mobj's address space) from which to perform the read.
 * @param offset An offset to be applied to base_addr.
 * @param dest The destination value.
 */
static bool pl_dwarf_read_u16 (plcrash_async_mobject_t *mobj, const plcrash_async_byteorder_t *byteorder,
                               pl_vm_address_t base_addr, pl_vm_off_t offset, uint16_t *dest)
{
    uint16_t *input = plcrash_async_mobject_remap_address(mobj, base_addr, offset, sizeof(uint16_t));
    if (input == NULL)
        return false;
    
    *dest = byteorder->swap16(*input);
    return true;
}

/**
 * @internal
 *
 * Read a 32-bit value.
 *
 * @param mobj Memory object from which to read the value.
 * @param byteorder Byte order of the target value.
 * @param base_addr The base address (within @a mobj's address space) from which to perform the read.
 * @param offset An offset to be applied to base_addr.
 * @param dest The destination value.
 */
static bool pl_dwarf_read_u32 (plcrash_async_mobject_t *mobj, const plcrash_async_byteorder_t *byteorder,
                               pl_vm_address_t base_addr, pl_vm_off_t offset, uint32_t *dest)
{
    uint32_t *input = plcrash_async_mobject_remap_address(mobj, base_addr, offset, sizeof(uint32_t));
    if (input == NULL)
        return false;

    *dest = byteorder->swap32(*input);
    return true;
}

/**
 * @internal
 *
 * Read a 64-bit value.
 *
 * @param mobj Memory object from which to read the value.
 * @param byteorder Byte order of the target value.
 * @param base_addr The base address (within @a mobj's address space) from which to perform the read.
 * @param offset An offset to be applied to base_addr.
 * @param dest The destination value.
 */
static bool pl_dwarf_read_u64 (plcrash_async_mobject_t *mobj, const plcrash_async_byteorder_t *byteorder,
                               pl_vm_address_t base_addr, pl_vm_off_t offset, uint64_t *dest)
{
    uint64_t *input = plcrash_async_mobject_remap_address(mobj, base_addr, offset, sizeof(uint64_t));
    if (input == NULL)
        return false;
    
    *dest = byteorder->swap64(*input);
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
        if (!pl_dwarf_read_u64(mobj, byteorder, base_addr, offset, &r64))
            return false;
        
        *dest = r64;
        return true;
    } else {
        uint32_t r32;
        if (!pl_dwarf_read_u32(mobj, byteorder, base_addr, offset, &r32))
            return false;
        *dest = r32;
        return true;
    }
}

/**
 * @}
 */