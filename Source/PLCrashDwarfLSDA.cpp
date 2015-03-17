/*
 * Author: Joe Ranieri <joe@alacatialabs.com>
 *
 * Copyright (c) 2015 Plausible Labs Cooperative, Inc.
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

#include "PLCrashDwarfLSDA.hpp"
#include "PLCrashAsyncDwarfPrimitives.hpp"

#if PLCRASH_FEATURE_UNWIND_DWARF

using namespace plcrash;
using namespace plcrash::async;

/**
 * Determines how many bytes are required by a given DWARF pointer encoding.
 */
template <typename machine_ptr>
static plcrash_error_t dwarf_encoding_size (uint8_t encoding, uint8_t *size)
{
    if (encoding == DW_EH_PE::DW_EH_PE_omit) {
        *size = 0;
        return PLCRASH_ESUCCESS;
    }

    switch (encoding & 0x0F) {
    case DW_EH_PE::DW_EH_PE_absptr:
        *size = sizeof(machine_ptr);
        return PLCRASH_ESUCCESS;
    case DW_EH_PE::DW_EH_PE_udata2:
        *size = sizeof(uint16_t);
        return PLCRASH_ESUCCESS;
    case DW_EH_PE::DW_EH_PE_udata4:
        *size = sizeof(uint32_t);
        return PLCRASH_ESUCCESS;
    case DW_EH_PE::DW_EH_PE_udata8:
        *size = sizeof(uint64_t);
        return PLCRASH_ESUCCESS;
    case DW_EH_PE::DW_EH_PE_sdata2:
        *size = sizeof(int16_t);
        return PLCRASH_ESUCCESS;
    case DW_EH_PE::DW_EH_PE_sdata4:
        *size = sizeof(int32_t);
        return PLCRASH_ESUCCESS;
    case DW_EH_PE::DW_EH_PE_sdata8:
        *size = sizeof(int64_t);
        return PLCRASH_ESUCCESS;
    default:
        return PLCRASH_EINVALID_DATA;
    }
}

/**
 * Decodes all of the action records associated with a single callsite.
 */
template <typename machine_ptr>
static plcrash_error_t read_action_table (plcrash_async_mobject_t *mobj,
                                          pl_vm_address_t first,
                                          std::vector<dwarf_lsda_action_record> &result,
                                          gnu_ehptr_reader<machine_ptr> *ptr_reader,
                                          pl_vm_address_t ttype_base,
                                          DW_EH_PE ttype_encoding)
{
    plcrash_error_t err = PLCRASH_ESUCCESS;

    /* All items in the types table have the same encoding and length, allowing
     * random access into it. */
    uint8_t ttype_size;
    if ((err = dwarf_encoding_size<machine_ptr>(ttype_encoding, &ttype_size)) != PLCRASH_ESUCCESS) {
        PLCF_DEBUG("Invalid ttype_encoding in LSDA: %i", ttype_encoding);
        return err;
    }

    /* Each action record is a type filter and the relative offset to the next
     * action record. An offset of zero means there are no more records. */
    pl_vm_address_t address = first;
    while (true) {
        pl_vm_off_t offset = 0;
        pl_vm_size_t size = 0;

        int64_t filter;
        if ((err = plcrash_async_dwarf_read_sleb128(mobj, address, offset, &filter, &size)) != PLCRASH_ESUCCESS) {
            PLCF_DEBUG("Failed to read action record filter");
            return err;
        }
        offset += size;

        int64_t next_offset;
        if ((err = plcrash_async_dwarf_read_sleb128(mobj, address, offset, &next_offset, &size)) != PLCRASH_ESUCCESS) {
            PLCF_DEBUG("Failed to read action record offset");
            return err;
        }

        /* If the filter is positive, this represents a simple catch. If the
         * filter is negative, it's an exception specification which can have
         * an arbitrary number of types. */
        dwarf_lsda_action_record action;
        if (filter > 0) {
            machine_ptr type_info = 0;
            if ((err = ptr_reader->read(mobj, ttype_base - (filter * ttype_size), 0, ttype_encoding, &type_info, &size)) != PLCRASH_ESUCCESS) {
                PLCF_DEBUG("Failed to dereference type info (filter = %lli)", filter);
                return err;
            }
            action.kind = DWARF_LSDA_ACTION_KIND_CATCH;
            action.types.push_back(type_info);
        } else {
            /* The exception specification type indexes list is a series of
             * uleb128s that is terminated by 0. The filter we're given is a
             * byte offset into the table (not an index). */
            pl_vm_off_t spec_offset = -filter - 1;
            action.kind = DWARF_LSDA_ACTION_KIND_EXCEPTION_SPECIFICATION;
            while (true) {
                uint64_t filter_index;
                if ((err = plcrash_async_dwarf_read_uleb128(mobj, ttype_base, spec_offset, &filter_index, &size)) != PLCRASH_ESUCCESS) {
                    PLCF_DEBUG("Failed to read action record filter");
                    return err;
                }
                spec_offset += size;

                if (filter_index == 0)
                    break;

                machine_ptr type_info = 0;
                if ((err = ptr_reader->read(mobj, ttype_base - (filter_index * ttype_size), 0, ttype_encoding, &type_info, &size)) != PLCRASH_ESUCCESS) {
                    PLCF_DEBUG("Failed to dereference type info (filter = %lli)", filter_index);
                    return err;
                }
                action.types.push_back(type_info);
            }
        }
        result.push_back(action);

        if (next_offset == 0)
            break;
        address = address + offset + next_offset;
    }

    return PLCRASH_ESUCCESS;
}

/**
 * Decode LSDA info at target-relative @a address.
 *
 * Any resources held by a successfully initialized instance must be freed via
 * plcrash_async_dwarf_lsda_info_free();
 *
 * @param info The LSDA record to be initialized.
 * @param mobj The memory object containing the LSDA at the start address.
 * @param byteorder The byte order of the data referenced by @a mobj.
 * @param address The target-relative address containing the LSDA info to be
 * decoded.
 */
template <typename machine_ptr>
plcrash_error_t plcrash::dwarf_lsda_info_init (dwarf_lsda_info_t *info,
                                               plcrash_async_mobject_t *mobj,
                                               const plcrash_async_byteorder_t *byteorder,
                                               pl_vm_address_t address)
{
    plcrash_error_t err;
    pl_vm_size_t size;
    pl_vm_off_t offset = 0;
    gnu_ehptr_reader<machine_ptr> ptr_reader(byteorder);

    DW_EH_PE lp_start_encoding = DW_EH_PE_omit;
    if ((err = plcrash_async_mobject_read_uint8(mobj, address, offset, (uint8_t *)&lp_start_encoding)) != PLCRASH_ESUCCESS) {
        PLCF_DEBUG("Invalid lp_start_encoding in lsda");
        return err;
    }
    offset += 1;

    if (lp_start_encoding != DW_EH_PE_omit) {
        machine_ptr ptr = 0;
        if ((err = ptr_reader.read(mobj, address, offset, lp_start_encoding, &ptr, &size)) != PLCRASH_ESUCCESS) {
            PLCF_DEBUG("Couldn't read lp_start (encoding %i)", lp_start_encoding);
            return err;
        }
        info->has_lp_start = true;
        info->lp_start = ptr;
        offset += size;
    } else {
        info->has_lp_start = false;
        info->lp_start = 0;
    }

    DW_EH_PE ttype_encoding = DW_EH_PE_omit;
    if ((err = plcrash_async_mobject_read_uint8(mobj, address, offset, (uint8_t *)&ttype_encoding)) != PLCRASH_ESUCCESS) {
        PLCF_DEBUG("Couldn't read ttype_encoding");
        return err;
    }
    offset += 1;

    pl_vm_address_t ttype_base = 0;
    if (ttype_encoding != DW_EH_PE_omit) {
        uint64_t ttypeOffset = 0;
        if ((err = plcrash_async_dwarf_read_uleb128(mobj, address, offset, &ttypeOffset, &size)) != PLCRASH_ESUCCESS) {
            PLCF_DEBUG("Couldn't read ttype_base");
            return err;
        }
        offset += size;
        ttype_base = address + offset + ttypeOffset;
    }

    DW_EH_PE call_site_encoding = DW_EH_PE_omit;
    if ((err = plcrash_async_mobject_read_uint8(mobj, address, offset, (uint8_t *)&call_site_encoding)) != PLCRASH_ESUCCESS) {
        PLCF_DEBUG("Couldn't read call_site_encoding");
        return err;
    }
    offset += 1;

    uint64_t call_site_table_length = 0;
    if ((err = plcrash_async_dwarf_read_uleb128(mobj, address, offset, &call_site_table_length, &size)) != PLCRASH_ESUCCESS) {
        PLCF_DEBUG("Couldn't read call_site_table_length");
        return err;
    }
    offset += size;

    /* After all of that header information is the callsite table, which encodes
     * what should happen for ranges of code in the function. We do not know
     * how many items are in it, but we do know its total length. */
    uint64_t call_site_start = offset;
    while (offset - call_site_start < call_site_table_length) {
        dwarf_lsda_callsite_info_t callsite;

        machine_ptr ptr = 0;
        if ((err = ptr_reader.read(mobj, address, offset, call_site_encoding, &ptr, &size)) != PLCRASH_ESUCCESS) {
            PLCF_DEBUG("Couldn't read call site's start");
            return err;
        }
        /* Despite what the spec says, this pointer appears to be relative to
         * the start of the function and not the previous callsite. */
        callsite.start = ptr;
        offset += size;

        if ((err = ptr_reader.read(mobj, address, offset, call_site_encoding, &ptr, &size)) != PLCRASH_ESUCCESS) {
            PLCF_DEBUG("Couldn't read call site's length");
            return err;
        }
        callsite.length = ptr;
        offset += size;

        if ((err = ptr_reader.read(mobj, address, offset, call_site_encoding, &ptr, &size)) != PLCRASH_ESUCCESS) {
            PLCF_DEBUG("Couldn't read call site's landing pad offset");
            return err;
        }
        callsite.landing_pad_offset = ptr;
        offset += size;

        uint8_t action = 0;
        if ((err = plcrash_async_mobject_read_uint8(mobj, address, offset, &action)) != PLCRASH_ESUCCESS) {
            PLCF_DEBUG("Couldn't read call site's action");
            return err;
        }
        offset += 1;

        /* The action is a 1-based index into the actions table. If the value is
         * zero and the landing pad is zero, the range of code is assumed not
         * to throw. If the value is zero and there is a landing pad, this is
         * a cleanup. */
        if (action > 0) {
            pl_vm_address_t action_table = address + call_site_start + call_site_table_length;
            err = read_action_table(mobj, action_table + action - 1, callsite.actions, &ptr_reader, ttype_base, ttype_encoding);
            if (err != PLCRASH_ESUCCESS)
                return err;
        }

        info->callsites.push_back(callsite);
    }

    return PLCRASH_ESUCCESS;
}

void plcrash::dwarf_lsda_info_free (dwarf_lsda_info_t *info)
{
    /* All resources will automatically be freed by the destructor. */
}

namespace plcrash {
template
plcrash_error_t dwarf_lsda_info_init<uint32_t> (dwarf_lsda_info_t *info,
                                                plcrash_async_mobject_t *mobj,
                                                const plcrash_async_byteorder_t *byteorder,
                                                pl_vm_address_t address);
template
plcrash_error_t dwarf_lsda_info_init<uint64_t> (dwarf_lsda_info_t *info,
                                                plcrash_async_mobject_t *mobj,
                                                const plcrash_async_byteorder_t *byteorder,
                                                pl_vm_address_t address);
}

#endif /* PLCRASH_FEATURE_UNWIND_DWARF */
