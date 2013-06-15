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

#include "PLCrashAsyncDwarfPrimitives.h"
#include "PLCrashAsyncDwarfEncoding.h"

#include <inttypes.h>

/**
 * @internal
 * @ingroup plcrash_async_dwarf
 * @{
 */

/**
 * Initialize GNU eh_frame pointer @a state. This is the base state to which DW_EH_PE_t encoded pointer values will be applied.
 *
 * @param state The state value to be initialized.
 * @param address_size The pointer size of the target system, in bytes; must be one of 1, 2, 4, or 8.
 *
 * Any resources held by a successfully initialized instance must be freed via plcrash_async_dwarf_gnueh_ptr_state_free();
 */
void plcrash_async_dwarf_gnueh_ptr_state_init (plcrash_async_dwarf_gnueh_ptr_state_t *state, uint8_t address_size) {
    PLCF_ASSERT(address_size == 1 || address_size == 2 || address_size == 4 || address_size == 8);
    
    state->address_size = address_size;
    state->frame_section_base = PLCRASH_ASYNC_DWARF_INVALID_BASE_ADDR;
    state->frame_section_vm_addr = PLCRASH_ASYNC_DWARF_INVALID_BASE_ADDR;
    state->pc_rel_base = PLCRASH_ASYNC_DWARF_INVALID_BASE_ADDR;
    state->text_base = PLCRASH_ASYNC_DWARF_INVALID_BASE_ADDR;
    state->data_base = PLCRASH_ASYNC_DWARF_INVALID_BASE_ADDR;
    state->func_base = PLCRASH_ASYNC_DWARF_INVALID_BASE_ADDR;
}

/**
 * Set the DW_EH_PE_pcrel base address.
 *
 * @param state State instance to modify.
 * @param pc_rel_base PC-relative base address to be applied to DW_EH_PE_pcrel offsets, or PLCRASH_ASYNC_DWARF_INVALID_BASE_ADDR. In the case of FDE
 * entries, this should be the address of the FDE entry itself.
 */
void plcrash_async_dwarf_gnueh_ptr_state_set_pc_rel_base (plcrash_async_dwarf_gnueh_ptr_state_t *state, uint64_t pc_rel_base) {
    state->pc_rel_base = pc_rel_base;
}

/**
 * Set the DW_EH_PE_aligned base addresses.
 *
 * @param state State instance to modify.
 * @param frame_section_base The base address (in-memory) of the loaded debug_frame or eh_frame section, or PLCRASH_ASYNC_DWARF_INVALID_BASE_ADDR. This is
 * used to calculate the offset of DW_EH_PE_aligned from the start of the frame section. This address should be the
 * actual base address at which the section has been mapped.
 *
 * @param frame_section_vm_addr The base VM address of the eh_frame or debug_frame section, or PLCRASH_ASYNC_DWARF_INVALID_BASE_ADDR.
 * This is used to calculate alignment for DW_EH_PE_aligned-encoded values. This address should be the aligned base VM
 * address at which the section will (or has been loaded) during execution, and will be used to calculate
 * DW_EH_PE_aligned alignment.
 */
void plcrash_async_dwarf_gnueh_ptr_state_set_frame_section_base (plcrash_async_dwarf_gnueh_ptr_state_t *state, uint64_t frame_section_base, uint64_t frame_section_vm_addr) {
    state->frame_section_base = frame_section_base;
    state->frame_section_vm_addr = frame_section_vm_addr;
}

/**
 * Set the DW_EH_PE_textrel base address.
 *
 * @param state State instance to modify.
 *
 * @param text_base The base address of the text segment to be applied to DW_EH_PE_textrel offsets, or PLCRASH_ASYNC_DWARF_INVALID_BASE_ADDR.
 */
void plcrash_async_dwarf_gnueh_ptr_state_set_text_base (plcrash_async_dwarf_gnueh_ptr_state_t *state, uint64_t text_base) {
    state->text_base = text_base;
}

/**
 * Set the DW_EH_PE_datarel base address.
 *
 * @param state State instance to modify.
 * @param data_base The base address of the data segment to be applied to DW_EH_PE_datarel offsets, or PLCRASH_ASYNC_DWARF_INVALID_BASE_ADDR.
 */
void plcrash_async_dwarf_gnueh_ptr_state_set_data_base (plcrash_async_dwarf_gnueh_ptr_state_t *state, uint64_t data_base) {
    state->data_base = data_base;
}

/**
 * Set the DW_EH_PE_funcrel base address.
 *
 * @param state State instance to modify.
 * @param func_base The base address of the function to be applied to DW_EH_PE_funcrel offsets, or PLCRASH_ASYNC_DWARF_INVALID_BASE_ADDR.
 */
void plcrash_async_dwarf_gnueh_ptr_state_set_func_base (plcrash_async_dwarf_gnueh_ptr_state_t *state, uint64_t func_base) {
    state->func_base = func_base;
}

/**
 * Free all resources associated with @a state.
 *
 * @param state A previously initialized state instance.
 */
void plcrash_async_dwarf_gnueh_ptr_state_free (plcrash_async_dwarf_gnueh_ptr_state_t *state) {
    // noop
}


/**
 * Read a GNU DWARF encoded pointer value from @a location within @a mobj. The encoding format is defined in
 * the Linux Standard Base Core Specification 4.1, section 10.5, DWARF Extensions.
 *
 * @param mobj The memory object from which the pointer data (including TEXT/DATA-relative values) will be read. This
 * should map the full binary that may be read; the pointer value may reference data that is relative to the binary
 * sections, depending on the base addresses supplied via @a state.
 * @param byteoder The byte order of the data referenced by @a mobj.
 * @param location A task-relative location within @a mobj.
 * @param offset An offset to apply to @a location.
 * @param encoding The encoding method to be used to decode the target pointer
 * @param state The base GNU eh_frame pointer state to which the encoded pointer value will be applied. If a value
 * is read that is relative to a @state-supplied base address of PLCRASH_ASYNC_DWARF_INVALID_BASE_ADDR, PLCRASH_ENOTSUP will be returned.
 * @param result On success, the pointer value.
 * @param size On success, will be set to the total size of the pointer data read at @a location, in bytes.
 */
plcrash_error_t plcrash_async_dwarf_read_gnueh_ptr (plcrash_async_mobject_t *mobj,
                                                    const plcrash_async_byteorder_t *byteorder,
                                                    pl_vm_address_t location,
                                                    pl_vm_off_t offset,
                                                    DW_EH_PE_t encoding,
                                                    plcrash_async_dwarf_gnueh_ptr_state_t *state,
                                                    uint64_t *result,
                                                    uint64_t *size)
{
    plcrash_error_t err;
    
    /* Skip DW_EH_pe_omit -- as per LSB 4.1.0, this signifies that no value is present */
    if (encoding == DW_EH_PE_omit) {
        PLCF_DEBUG("Skipping decoding of DW_EH_PE_omit pointer");
        return PLCRASH_ENOTFOUND;
    }
    
    /* Initialize the output size; we apply offsets to this size to allow for aligning the
     * address prior to reading the pointer data, etc. */
    *size = 0;
    
    /* Calculate the base address; bits 5-8 are used to specify the relative offset type */
    uint64_t base;
    switch (encoding & 0x70) {
        case DW_EH_PE_pcrel:
            if (state->pc_rel_base == PLCRASH_ASYNC_DWARF_INVALID_BASE_ADDR) {
                PLCF_DEBUG("Cannot decode DW_EH_PE_pcrel value with PLCRASH_ASYNC_DWARF_INVALID_BASE_ADDR pc_rel_base");
                return PLCRASH_ENOTSUP;
            }
            
            base = state->pc_rel_base;
            break;
            
        case DW_EH_PE_absptr:
            /* No flags are set */
            base = 0x0;
            break;
            
        case DW_EH_PE_textrel:
            if (state->text_base == PLCRASH_ASYNC_DWARF_INVALID_BASE_ADDR) {
                PLCF_DEBUG("Cannot decode DW_EH_PE_textrel value with PLCRASH_ASYNC_DWARF_INVALID_BASE_ADDR text_addr");
                return PLCRASH_ENOTSUP;
            }
            base = state->text_base;
            break;
            
        case DW_EH_PE_datarel:
            if (state->data_base == PLCRASH_ASYNC_DWARF_INVALID_BASE_ADDR) {
                PLCF_DEBUG("Cannot decode DW_EH_PE_datarel value with PLCRASH_ASYNC_DWARF_INVALID_BASE_ADDR data_base");
                return PLCRASH_ENOTSUP;
            }
            base = state->data_base;
            break;
            
        case DW_EH_PE_funcrel:
            if (state->func_base == PLCRASH_ASYNC_DWARF_INVALID_BASE_ADDR) {
                PLCF_DEBUG("Cannot decode DW_EH_PE_funcrel value with PLCRASH_ASYNC_DWARF_INVALID_BASE_ADDR func_base");
                return PLCRASH_ENOTSUP;
            }
            
            base = state->func_base;
            break;
            
        case DW_EH_PE_aligned: {
            /* Verify availability of required base addresses */
            if (state->frame_section_vm_addr == PLCRASH_ASYNC_DWARF_INVALID_BASE_ADDR) {
                PLCF_DEBUG("Cannot decode DW_EH_PE_aligned value with PLCRASH_ASYNC_DWARF_INVALID_BASE_ADDR frame_section_vm_addr");
                return PLCRASH_ENOTSUP;
            } else if (state->frame_section_base == PLCRASH_ASYNC_DWARF_INVALID_BASE_ADDR) {
                PLCF_DEBUG("Cannot decode DW_EH_PE_aligned value with PLCRASH_ASYNC_DWARF_INVALID_BASE_ADDR frame_section_base");
                return PLCRASH_ENOTSUP;
            }
            
            /* Compute the offset+alignment relative to the section base */
            PLCF_ASSERT(location >= state->frame_section_base);
            uint64_t offset = location - state->frame_section_base;
            
            /* Apply to the VM load address for the section. */
            uint64_t vm_addr = state->frame_section_vm_addr + offset;
            uint64_t vm_aligned = (vm_addr + (state->address_size-1)) & ~(state->address_size);
            
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
        case DW_EH_PE_absptr: {
            uint64_t u64;
            
            if ((err = plcrash_async_dwarf_read_uintmax64(mobj, byteorder, location, offset, state->address_size, &u64)) != PLCRASH_ESUCCESS) {
                PLCF_DEBUG("Failed to read value at 0x%" PRIx64, (uint64_t) location);
                return err;
            }
            
            *result = u64 + base;
            *size += state->address_size;
            break;
        }
            
        case DW_EH_PE_uleb128: {
            uint64_t ulebv;
            pl_vm_size_t uleb_size;
            
            if ((err = plcrash_async_dwarf_read_uleb128(mobj, location, offset, &ulebv, &uleb_size)) != PLCRASH_ESUCCESS) {
                PLCF_DEBUG("Failed to read uleb128 value at 0x%" PRIx64, (uint64_t) location);
                return err;
            }
            
            *result = ulebv + base;
            *size += uleb_size;
            break;
        }
            
        case DW_EH_PE_udata2: {
            uint16_t udata2;
            if ((err = plcrash_async_mobject_read_uint16(mobj, byteorder, location, offset, &udata2)) != PLCRASH_ESUCCESS) {
                PLCF_DEBUG("Failed to read udata2 value at 0x%" PRIx64, (uint64_t) location);
                return err;
            }
            
            *result = udata2 + base;
            *size += 2;
            break;
        }
            
        case DW_EH_PE_udata4: {
            uint32_t udata4;
            if ((err = plcrash_async_mobject_read_uint32(mobj, byteorder, location, offset, &udata4)) != PLCRASH_ESUCCESS) {
                PLCF_DEBUG("Failed to read udata4 value at 0x%" PRIx64, (uint64_t) location);
                return err;
            }

            *result = udata4 + base;
            *size += 4;
            break;
        }
            
        case DW_EH_PE_udata8: {
            uint64_t udata8;
            if ((err = plcrash_async_mobject_read_uint64(mobj, byteorder, location, offset, &udata8)) != PLCRASH_ESUCCESS) {
                PLCF_DEBUG("Failed to read udata8 value at 0x%" PRIx64, (uint64_t) location);
                return err;
            }

            *result = udata8 + base;
            *size += 8;
            break;
        }
            
        case DW_EH_PE_sleb128: {
            int64_t slebv;
            pl_vm_size_t sleb_size;
            
            if ((err = plcrash_async_dwarf_read_sleb128(mobj, location, offset, &slebv, &sleb_size)) != PLCRASH_ESUCCESS) {
                PLCF_DEBUG("Failed to read sleb128 value at 0x%" PRIx64, (uint64_t) location);
                return err;
            }
            
            *result = slebv + base;
            *size += sleb_size;
            break;
        }
            
        case DW_EH_PE_sdata2: {
            int16_t sdata2;

            if ((err = plcrash_async_mobject_read_uint16(mobj, byteorder, location, offset, (uint16_t *) &sdata2)) != PLCRASH_ESUCCESS) {
                PLCF_DEBUG("Failed to read sdata2 value at 0x%" PRIx64, (uint64_t) location);
                return err;
            }

            *result = sdata2 + base;
            *size += 2;
            break;
        }
            
        case DW_EH_PE_sdata4: {
            int32_t sdata4;
            if ((err = plcrash_async_mobject_read_uint32(mobj, byteorder, location, offset, (uint32_t *) &sdata4)) != PLCRASH_ESUCCESS) {
                PLCF_DEBUG("Failed to read sdata4 value at 0x%" PRIx64, (uint64_t) location);
                return err;
            }

            *result = sdata4 + base;
            *size += 4;
            break;
        }
            
        case DW_EH_PE_sdata8: {
            int64_t sdata8;
            if ((err = plcrash_async_mobject_read_uint64(mobj, byteorder, location, offset, (uint64_t *) &sdata8)) != PLCRASH_ESUCCESS) {
                PLCF_DEBUG("Failed to read sdata8 value at 0x%" PRIx64, (uint64_t) location);
                return err;
            }

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
    if (encoding & DW_EH_PE_indirect) {
        /* The size of the target doesn't matter; the caller only needs to know how many bytes were read from
         * @a location */
        uint64_t target_size;
        return plcrash_async_dwarf_read_gnueh_ptr(mobj, byteorder, *result, 0, DW_EH_PE_absptr, state, result, &target_size);
    }
    
    return PLCRASH_ESUCCESS;
}

#pragma mark Primitive Type Decoding

/**
 * Read a ULEB128 value from @a location within @a mobj.
 *
 * @param mobj The memory object from which the LEB128 data will be read.
 * @param location A task-relative location within @a mobj.
 * @param offset Offset to be applied to @a location.
 * @param result On success, the ULEB128 value.
 * @param size On success, will be set to the total size of the decoded LEB128 value at @a location, in bytes.
 */
plcrash_error_t plcrash_async_dwarf_read_uleb128 (plcrash_async_mobject_t *mobj, pl_vm_address_t location, pl_vm_off_t offset, uint64_t *result, pl_vm_size_t *size) {
    unsigned int shift = 0;
    pl_vm_off_t position = 0;
    *result = 0;
    
    uint8_t *p;
    while ((p = plcrash_async_mobject_remap_address(mobj, location, position + offset, 1)) != NULL) {
        /* LEB128 uses 7 bits for the number, the final bit to signal completion */
        uint8_t byte = *p;
        *result |= ((uint64_t) (byte & 0x7f)) << shift;
        shift += 7;
        
        /* This is used to track length, so we must set it before
         * potentially terminating the loop below */
        position++;
        
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
    
    *size = position;
    return PLCRASH_ESUCCESS;
}

/**
 * Read a SLEB128 value from @a location within @a mobj.
 *
 * @param mobj The memory object from which the LEB128 data will be read.
 * @param location A task-relative location within @a mobj.
 * @param offset Offset to be applied to @a location.
 * @param result On success, the ULEB128 value.
 * @param size On success, will be set to the total size of the decoded LEB128 value, in bytes.
 */
plcrash_error_t plcrash_async_dwarf_read_sleb128 (plcrash_async_mobject_t *mobj, pl_vm_address_t location, pl_vm_off_t offset, int64_t *result, pl_vm_size_t *size) {
    unsigned int shift = 0;
    pl_vm_off_t position = 0;
    *result = 0;
    
    uint8_t *p;
    while ((p = plcrash_async_mobject_remap_address(mobj, location, position + offset, 1)) != NULL) {
        /* LEB128 uses 7 bits for the number, the final bit to signal completion */
        uint8_t byte = *p;
        *result |= ((uint64_t) (byte & 0x7f)) << shift;
        shift += 7;
        
        /* This is used to track length, so we must set it before
         * potentially terminating the loop below */
        position++;
        
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
    
    *size = position;
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
plcrash_error_t plcrash_async_dwarf_read_uintmax64 (plcrash_async_mobject_t *mobj,
                                                    const plcrash_async_byteorder_t *byteorder,
                                                    pl_vm_address_t base_addr,
                                                    pl_vm_off_t offset,
                                                    uint8_t data_size,
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
        return PLCRASH_EINVAL;
    
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
            return PLCRASH_EINVAL;
    }
    
    return PLCRASH_ESUCCESS;
}

/**
 * @}
 */