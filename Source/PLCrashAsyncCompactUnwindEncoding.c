/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 * Author: Gwynne Raskind <gwynne@darkrainfall.org>
 *
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

#include "PLCrashAsyncCompactUnwindEncoding.h"

#include <inttypes.h>

/**
 * @internal
 * @ingroup plcrash_async
 * @defgroup plcrash_async_cfe Compact Frame Encoding
 *
 * Implements async-safe parsing of compact frame unwind encodings.
 * @{
 */

/**
 * Initialize a new CFE reader using the provided memory object. Any resources held by a fully or partially initialized
 * instance must be freed via plcrash_async_cfe_reader_free();
 *
 * @param reader The reader instance to initialize.
 * @param mobj The memory object containing CFE data at the start address. This instance must survive for the lifetime
 * of the reader.
 * @param cpu_type The target architecture of the CFE data, encoded as a Mach-O CPU type. Interpreting CFE data is
 * architecture-specific, and Apple has not defined encodings for all supported architectures.
 */
plcrash_error_t plcrash_async_cfe_reader_init (plcrash_async_cfe_reader_t *reader, plcrash_async_mobject_t *mobj, cpu_type_t cputype) {
    reader->mobj = mobj;
    reader->cputype = cputype;

    /* Determine the expected encoding */
    switch (cputype) {
        case CPU_TYPE_X86:
        case CPU_TYPE_X86_64:
            reader->byteorder = plcrash_async_byteorder_little_endian();
            break;

        default:
            PLCF_DEBUG("Unsupported CPU type: %" PRIu32, cputype);
            return PLCRASH_ENOTSUP;
    }

    /* Fetch and verify the header */
    pl_vm_address_t base_addr = plcrash_async_mobject_base_address(mobj);
    struct unwind_info_section_header *header = plcrash_async_mobject_remap_address(mobj, base_addr, sizeof(*header));
    if (header == NULL) {
        PLCF_DEBUG("Could not map the unwind info section header");
        return PLCRASH_EINVAL;
    }

    /* Verify the format version */
    uint32_t version = reader->byteorder->swap32(header->version);
    if (version != 1) {
        PLCF_DEBUG("Unsupported CFE version: %" PRIu32, version);
        return PLCRASH_ENOTSUP;
    }

    reader->header = *header;
    return PLCRASH_ESUCCESS;
}

/**
 * TODO
 *
 * @param reader The initialized CFE reader.
 * @param ip The instruction pointer to search for within the CFE data.
 */
plcrash_error_t plcrash_async_cfe_reader_find_ip (plcrash_async_cfe_reader_t *reader, pl_vm_address_t ip) {
    const plcrash_async_byteorder_t *byteorder = reader->byteorder;

    /* Find the index */
    struct unwind_info_section_header_index_entry *index_entry;
    uint32_t index_off = byteorder->swap32(reader->header.indexSectionOffset);
    uint32_t index_count = byteorder->swap32(reader->header.indexCount);
    
    // TODO - binary search for the index entry
    // TODO - unbounded index_count could trigger overflow
    pl_vm_address_t base_addr = plcrash_async_mobject_base_address(reader->mobj);
    index_entry = plcrash_async_mobject_remap_address(reader->mobj, base_addr + index_off, index_count * sizeof(*index_entry));
    if (index_entry == NULL) {
        PLCF_DEBUG("Could not map the full unwind info section index");
        return PLCRASH_EINVAL;
    }

    return PLCRASH_ESUCCESS;
}

/**
 * Free all resources associated with @a reader.
 */
void plcrash_async_cfe_reader_free (plcrash_async_cfe_reader_t *reader) {
    // noop
}

/**
 * @} plcrash_async_cfe
 */