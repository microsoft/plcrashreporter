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

#include "PLCrashAsyncDwarfPrivate.h"

#include <inttypes.h>

/**
 * @internal
 * @ingroup plcrash_async_dwarf
 * @defgroup plcrash_async_dwarf_private DWARF (Internal Implementation)
 *
 * Internal DWARF parsing.
 * @{
 */

static bool pl_dwarf_read_umax64 (plcrash_async_mobject_t *mobj, const plcrash_async_byteorder_t *byteorder,
                                  pl_vm_address_t base_addr, pl_vm_off_t offset, uint8_t data_size,
                                  uint64_t *dest);

/**
 * Parse a new DWARF CIE record using the provided memory object and initialize @a info.
 *
 * Any resources held by a successfully initialized instance must be freed via plcrash_async_dwarf_cie_info_free();
 *
 * @param info The CIE info instance to initialize.
 * @param mobj The memory object containing frame data (eh_frame or debug_frame) at the start address.
 * @param byteoder The byte order of the data referenced by @a mobj.
 * @param ptr_state The pointer state to be used when decoding GNU eh_frame pointer values.
 * @param address The task-relative address within @a mobj of the CIE to be decoded.
 */
plcrash_error_t plcrash_async_dwarf_cie_info_init (plcrash_async_dwarf_cie_info_t *info,
                                                   plcrash_async_mobject_t *mobj,
                                                   const plcrash_async_byteorder_t *byteorder,
                                                   plcrash_async_dwarf_gnueh_ptr_state_t *ptr_state,
                                                   pl_vm_address_t address)
{
    pl_vm_address_t base_addr = plcrash_async_mobject_base_address(mobj);
    pl_vm_address_t offset = 0;
    plcrash_error_t err;

    /* Extract and save the FDE length */
    bool m64;
    pl_vm_size_t length_size;
    {
        uint32_t length32;
        
        if (plcrash_async_mobject_read_uint32(mobj, byteorder, address, 0x0, &length32) != PLCRASH_ESUCCESS) {
            PLCF_DEBUG("CIE 0x%" PRIx64 " header lies outside the mapped range", (uint64_t) address);
            return PLCRASH_EINVAL;
        }
        
        if (length32 == UINT32_MAX) {
            if ((err = plcrash_async_mobject_read_uint64(mobj, byteorder, address, sizeof(uint32_t), &info->cie_length)) != PLCRASH_ESUCCESS)
                return err;
            
            length_size = sizeof(uint64_t) + sizeof(uint32_t);
            m64 = true;
        } else {
            info->cie_length = length32;
            length_size = sizeof(uint32_t);
            m64 = false;
        }
    }
    
    /* Save the CIE offset; this is the CIE address, relative to the section base address, not including
     * the CIE initial length. */
    PLCF_ASSERT(address >= base_addr);
    info->cie_offset = (address - base_addr) + length_size;
    offset += length_size;

    /* Fetch the cie_id. This is either 32-bit or 64-bit */
    if (m64) {
        if ((err = plcrash_async_mobject_read_uint64(mobj, byteorder, address, offset, &info->cie_id)) != PLCRASH_ESUCCESS) {
            PLCF_DEBUG("CIE id could not be read");
            return err;
        }
        
        offset += sizeof(uint64_t);
    } else {
        uint32_t u32;
        if ((err = plcrash_async_mobject_read_uint32(mobj, byteorder, address, offset, &u32)) != PLCRASH_ESUCCESS) {
            PLCF_DEBUG("CIE id could not be read");
            return err;
        }
        info->cie_id = u32;
        offset += sizeof(uint32_t);
    }

    /* Sanity check the CIE id; it should always be 0 (eh_frame) or UINT?_MAX (debug_frame) */
    if (info->cie_id != 0 && ((!m64 && info->cie_id != UINT32_MAX) || (m64 && info->cie_id != UINT64_MAX)))  {
        PLCF_DEBUG("CIE id is not one of 0 (eh_frame) or UINT?_MAX (debug_frame): %" PRIx64, info->cie_id);
        return PLCRASH_EINVAL;
    }
    
    
    /* Fetch and sanity check the version; it should either be 1 (eh_frame), 3 (DWARF3 debug_frame), or 4 (DWARF4 debug_frame) */
    if ((err = plcrash_async_mobject_read_uint8(mobj, address, offset, &info->cie_version)) != PLCRASH_ESUCCESS) {
        PLCF_DEBUG("CIE version could not be read");
        return err;
    }
    
    if (info->cie_version != 1 && info->cie_version != 3 && info->cie_version != 4) {
        PLCF_DEBUG("CIE id is not one of 1 (eh_frame) or 3 (DWARF3) or 4 (DWARF4): %" PRIu8, info->cie_version);
        return PLCRASH_EINVAL;
    }
    
    offset += sizeof(uint8_t);
    
    /* Save the start and end of the augmentation data; we'll parse the string below. */
    pl_vm_address_t augment_offset = offset;
    pl_vm_size_t augment_size = 0;
    {
        uint8_t augment_char;
        while (augment_size < PL_VM_SIZE_MAX && (err = plcrash_async_mobject_read_uint8(mobj, address, augment_offset+augment_size, &augment_char)) == PLCRASH_ESUCCESS) {
            /* Check for an unknown augmentation string. See the parsing section below for more details. If the augmentation
             * string is not of the expected format (or empty), we can't parse a useful subset of the CIE */
            if (augment_size == 0) {
                if (augment_char == 'z') {
                    info->has_eh_augmentation = true;
                } else if (augment_char != '\0') {
                    PLCF_DEBUG("Unknown augmentation string prefix of %c, cannot parse CIE", augment_char);
                    return PLCRASH_ENOTSUP;
                }
            }

            /* Adjust the calculated size */
            augment_size++;

            /* Check for completion */
            if (augment_char == '\0')
                break;
        }
        
        if (err != PLCRASH_ESUCCESS) {
            PLCF_DEBUG("CIE augmentation string could not be read");
            return err;
        }
        
        if (augment_size == PL_VM_SIZE_MAX) {
            /* This is pretty much impossible */
            PLCF_DEBUG("CIE augmentation string was too long");
            return err;
        }
        
        offset += augment_size;
    }
    // pl_vm_address_t augment_end = augment_offset + augment_size;
    
    /* Fetch the DWARF 4-only fields. */
    if (info->cie_version == 4) {
        if ((err = plcrash_async_mobject_read_uint8(mobj, address, offset, &info->address_size)) != PLCRASH_ESUCCESS) {
            PLCF_DEBUG("CIE address_size could not be read");
            return err;
        }
        offset += sizeof(uint8_t);
        
        if ((err = plcrash_async_mobject_read_uint8(mobj, address, offset, &info->segment_size)) != PLCRASH_ESUCCESS) {
            PLCF_DEBUG("CIE segment_size could not be read");
            return err;
        }
        offset += sizeof(uint8_t);
    }
    
    /* Fetch the code alignment factor */
    pl_vm_size_t leb_size;
    if ((err = plcrash_async_dwarf_read_uleb128(mobj, address, offset, &info->code_alignment_factor, &leb_size)) != PLCRASH_ESUCCESS) {
        PLCF_DEBUG("Failed to read CIE code alignment value");
        return err;
    }
    
    offset += leb_size;
    
    /* Fetch the data alignment factor */
    if ((err = plcrash_async_dwarf_read_sleb128(mobj, address, offset, &info->data_alignment_factor, &leb_size)) != PLCRASH_ESUCCESS) {
        PLCF_DEBUG("Failed to read CIE data alignment value");
        return err;
    }
    
    offset += leb_size;
    
    /* Fetch the return address register */
    if ((err = plcrash_async_dwarf_read_uleb128(mobj, address, offset, &info->return_address_register, &leb_size)) != PLCRASH_ESUCCESS) {
        PLCF_DEBUG("Failed to read CIE return address register");
        return err;
    }

    offset += leb_size;
    
    /*
     * Parse the augmentation string (and associated data); the definition of the augmentation string is left to implementors. Most
     * entities, including Apple, use the augmentation values defined by GCC and documented in the the LSB Core Standard -- we document
     * the supported flags here, but refer to LSB 4.1.0 Section 10.6.1.1.1 for more details.
     *
     * According to the DWARF specification (see DWARF4, Section 6.4.1), only a few fields are readable if an unknown augmentation
     * string is parsed. Since the augmentation string may define additional fields or data values in the CIE/FDE, the inclusion
     * of an unknown value makes most of the structure unparsable. However, GCC has defined an additional augmentation value, which,
     * if included as the first byte in the augmentation string, will define the total length of associated augmentation data; in
     * that case, one could skip over the full set of augmentation data, and safely parse the remainder of the structure.
     *
     * That said, an unknown augmentation flag will prevent reading of the remainder of the augmentation field, which
     * may result in the CIE being unusable. Ideally, future toolchains will only append new flag types to the end of the
     * string, allowing all known data to be read first. Given this, we terminate parsing upon hitting an unknown string
     * and leave the remainder of the augmentation data flags unset in our parsed info record.
     *
     * Supported augmentation flags (as defined by the LSB 4.1.0 Section 10.6.1.1.1):
     *
     *  'z': If present as the first character of the string, the the GCC Augmentation Data shall be present and the augmentation
     *       string and data be interpreted according to the LSB specification. The first field of the augmentation data will be
     *       a ULEB128 length value, allowing our parser to skip over the entire augmentation data field. In the case of
     *       unknown augmentation flags, the parser may still safely read past the entire Augmentation Data field, and the
     *       field constraints of DWARF4 Section 6.4.1 no longer apply.
     *
     *       If this value is not present, no other LSB-defined augmentation values may be parsed.
     *
     *  'L': May be present at any position after the first character of the augmentation string, but only if 'z' is
     *       the first character of the string. If present, it indicates the presence of one argument in the Augmentation Data
     *       of the CIE, and a corresponding argument in the Augmentation Data of the FDE. The argument in the Augmentation Data
     *       of the CIE is 1-byte and represents the pointer encoding used for the argument in the Augmentation Data of the FDE,
     *       which is the address of a language-specific data area (LSDA). The size of the LSDA pointer is specified by the pointer
     *       encoding used.
     *
     *  'P': May be present at any position after the first character of the augmentation string, but only if 'z' is
     *       the first character of the string. If present, it indicates the presence of two arguments in the Augmentation
     *       Data of the CIE. The first argument is 1-byte and represents the pointer encoding used for the second argument,
     *       which is the address of a personality routine handler. The personality routine is used to handle language and
     *       vendor-specific tasks. The system unwind library interface accesses the language-specific exception handling
     *       semantics via the pointer to the personality routine. The personality routine does not have an ABI-specific name.
     *       The size of the personality routine pointer is specified by the pointer encoding used.
     *
     *  'R': May be present at any position after the first character of the augmentation string, but only if 'z' is
     *       the first character of the string. If present, The Augmentation Data shall include a 1 byte argument that
     *       represents the pointer encoding for the address pointers used in the FDE.
     *
     *  'S': This is not documented by the LSB eh_frame specification. This value designates the frame as a signal
     *       frame, which may require special handling on some architectures/ABIs. This value is poorly documented, but
     *       seems to be unused on Mac OS X and iOS. The best available 'documentation' may be found in GCC's bugzilla:
     *         http://gcc.gnu.org/bugzilla/show_bug.cgi?id=26208
     */
    uint64_t augment_data_size = 0;
    if (info->has_eh_augmentation) {
        /* Fetch the total augmentation data size */
        if ((err = plcrash_async_dwarf_read_uleb128(mobj, address, offset, &augment_data_size, &leb_size)) != PLCRASH_ESUCCESS) {
            PLCF_DEBUG("Failed to read the augmentation data uleb128 length");
            return err;
        }

        /* Determine the read position for the augmentation data */
        pl_vm_size_t data_offset = offset + leb_size;
        
        /* Iterate the entries, skipping the initial 'z' */
        for (pl_vm_size_t i = 1; i < augment_size; i++) {
            bool terminate = false;

            /* Fetch the next flag */
            uint8_t uc;
            if ((err = plcrash_async_mobject_read_uint8(mobj, address, augment_offset+i, &uc)) != PLCRASH_ESUCCESS) {
                PLCF_DEBUG("Failed to read CIE augmentation data");
                return err;
            }
    
            switch (uc) {
                case 'L':
                    /* Read the LSDA encoding */
                    if ((err = plcrash_async_mobject_read_uint8(mobj, address, data_offset, &info->eh_augmentation.lsda_encoding)) != PLCRASH_ESUCCESS) {
                        PLCF_DEBUG("Failed to read the LSDA encoding value");
                        return err;
                    }
                    
                    info->eh_augmentation.has_lsda_encoding = true;
                    data_offset += sizeof(uint8_t);
                    break;
                    
                case 'P': {
                    uint8_t ptr_enc;
                    pl_vm_size_t size;
                    
                    /* Read the personality routine pointer encoding */
                    if ((err = plcrash_async_mobject_read_uint8(mobj, address, data_offset, &ptr_enc)) != PLCRASH_ESUCCESS) {
                        PLCF_DEBUG("Failed to read the personality routine encoding value");
                        return err;
                    }
                    
                    data_offset += sizeof(uint8_t);

                    /* Read the actual pointer value */
                    err = plcrash_async_dwarf_read_gnueh_ptr(mobj, byteorder, address, data_offset, ptr_enc, ptr_state,
                                                             &info->eh_augmentation.personality_address, &size);
                    if (err != PLCRASH_ESUCCESS) {
                        PLCF_DEBUG("Failed to read the personality routine pointer value");
                        return err;
                    }

                    info->eh_augmentation.has_personality_address = true;
                    data_offset += size;                    
                    break;
                }

                case 'R':
                    /* Read the pointer encoding */
                    if ((err = plcrash_async_mobject_read_uint8(mobj, address, data_offset, &info->eh_augmentation.pointer_encoding)) != PLCRASH_ESUCCESS) {
                        PLCF_DEBUG("Failed to read the LSDA encoding value");
                        return err;
                    }
                    
                    info->eh_augmentation.has_pointer_encoding = true;
                    data_offset += sizeof(uint8_t);
                    break;
                    break;
                    
                case 'S':
                    info->eh_augmentation.signal_frame = true;
                    break;

                case '\0':
                    break;

                default:
                    PLCF_DEBUG("Unknown augmentation entry of %c; terminating parsing early", uc);
                    terminate = true;
                    break;
            }
            
            if (terminate)
                break;
        }
    }
    
    /* Skip all (possibly partially parsed) augmentation data */
    offset += augment_data_size;
    
    /* Save the initial instructions offset */
    info->initial_instructions_offset = offset;

    return PLCRASH_ESUCCESS;
}

/**
 *
 */
void plcrash_async_dwarf_cie_info_free (plcrash_async_dwarf_cie_info_t *info) {
    
}

/**
 * Initialize GNU eh_frame pointer @a state. This is the base state to which DW_EH_PE_t encoded pointer values will be applied.
 *
 * @param state The state value to be initialized.
 * @param address_size The pointer size of the target system, in bytes; must be one of 1, 2, 4, or 8.
 * @param frame_section_base The base address (in-memory) of the loaded debug_frame or eh_frame section, or PLCRASH_ASYNC_DWARF_INVALID_BASE_ADDR. This is
 * used to calculate the offset of DW_EH_PE_aligned from the start of the frame section. This address should be the
 * actual base address at which the section has been mapped.
 *
 * @param frame_section_vm_addr The base VM address of the eh_frame or debug_frame section, or PLCRASH_ASYNC_DWARF_INVALID_BASE_ADDR.
 * This is used to calculate alignment for DW_EH_PE_aligned-encoded values. This address should be the aligned base VM
 * address at which the section will (or has been loaded) during execution, and will be used to calculate
 * DW_EH_PE_aligned alignment.
 *
 * @param pc_rel_base PC-relative base address to be applied to DW_EH_PE_pcrel offsets, or PLCRASH_ASYNC_DWARF_INVALID_BASE_ADDR. In the case of FDE
 * entries, this should be the address of the FDE entry itself.
 * @param text_base The base address of the text segment to be applied to DW_EH_PE_textrel offsets, or PLCRASH_ASYNC_DWARF_INVALID_BASE_ADDR.
 * @param data_base The base address of the data segment to be applied to DW_EH_PE_datarel offsets, or PLCRASH_ASYNC_DWARF_INVALID_BASE_ADDR.
 * @param func_base The base address of the function to be applied to DW_EH_PE_funcrel offsets, or PLCRASH_ASYNC_DWARF_INVALID_BASE_ADDR.
 *
 * Any resources held by a successfully initialized instance must be freed via plcrash_async_dwarf_gnueh_ptr_state_free();
 */
void plcrash_async_dwarf_gnueh_ptr_state_init (plcrash_async_dwarf_gnueh_ptr_state_t *state,
                                               uint8_t address_size,
                                               uint64_t frame_section_base,
                                               uint64_t frame_section_vm_addr,
                                               uint64_t pc_rel_base,
                                               uint64_t text_base,
                                               uint64_t data_base,
                                               uint64_t func_base)
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
            
            if (!pl_dwarf_read_umax64(mobj, byteorder, location, offset, state->address_size, &u64)) {
                PLCF_DEBUG("Failed to read value at 0x%" PRIx64, (uint64_t) location);
                return PLCRASH_EINVAL;
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
        pl_vm_size_t target_size;
        return plcrash_async_dwarf_read_gnueh_ptr(mobj, byteorder, *result, 0, DW_EH_PE_absptr, state, result, &target_size);
    }
    
    return PLCRASH_ESUCCESS;
}

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
static bool pl_dwarf_read_umax64 (plcrash_async_mobject_t *mobj, const plcrash_async_byteorder_t *byteorder,
                                  pl_vm_address_t base_addr, pl_vm_off_t offset, uint8_t data_size,
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
 * @}
 */