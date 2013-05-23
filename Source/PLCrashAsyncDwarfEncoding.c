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


/**
 * Initialize a new DWARF frame reader using the provided memory object. Any resources held by a successfully initialized
 * instance must be freed via plcrash_async_dwarf_frame_reader_free();
 *
 * @param reader The reader instance to initialize.
 * @param mobj The memory object containing frame data (eh_frame or debug_frame) at the start address. This instance must
 * survive for the lifetime of the reader.
 * @param byteoder The byte order of the data referenced by @a mobj.
 */
plcrash_error_t plcrash_async_dwarf_frame_reader_init (plcrash_async_dwarf_frame_reader_t *reader, plcrash_async_mobject_t *mobj, const plcrash_async_byteorder_t *byteorder) {
    reader->mobj = mobj;
    reader->byteorder = byteorder;

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
 *
 * @return Returns PLFRAME_ESUCCCESS on success, or one of the remaining error codes if a DWARF parsing error occurs. If
 * the entry can not be found, PLFRAME_ENOTFOUND will be returned.
 */
plcrash_error_t plcrash_async_dwarf_frame_reader_find_fde (plcrash_async_dwarf_frame_reader_t *reader, pl_vm_size_t offset, pl_vm_address_t pc) {
    //const plcrash_async_byteorder_t *byteorder = reader->byteorder;
    const pl_vm_address_t base_addr = plcrash_async_mobject_base_address(reader->mobj);
    const pl_vm_address_t end_addr = base_addr + plcrash_async_mobject_length(reader->mobj);

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
        /* Fetch the entry length */
        uint64_t length;
        uint32_t *length32 = plcrash_async_mobject_remap_address(reader->mobj, cfi_entry, 0x0, sizeof(uint32_t));
        if (length32 == NULL) {
            PLCF_DEBUG("The current CFI entry 0x%" PRIx64 " header lies outside the mapped range", (uint64_t) cfi_entry);
            return PLCRASH_EINVAL;
        }
        
        if (*length32 == 0xffffffff) {
            uint64_t *length64 = plcrash_async_mobject_remap_address(reader->mobj, cfi_entry, 4, sizeof(uint64_t));
            if (length64 == NULL) {
                PLCF_DEBUG("The current CFI entry 0x%" PRIx64 " header lies outside the mapped range", (uint64_t) cfi_entry);
                return PLCRASH_EINVAL;
            }

            length = *length64;
        } else {
            length = *length32;
        }

        /* Check for end marker */
        if (length == 0x0)
            return PLCRASH_ENOTFOUND;
        
        /* Skip over entry */
        if (!plcrash_async_address_apply_offset(cfi_entry, length, &cfi_entry)) {
            PLCF_DEBUG("Entry length overflows the CFI address");
            return PLCRASH_EINVAL;
        }
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
 * @}
 */