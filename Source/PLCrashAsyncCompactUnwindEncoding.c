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

#include "PLCrashAsyncCompactUnwindEncoding.h"

#include <inttypes.h>
#include <mach-o/compact_unwind_encoding.h>

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
 */
plcrash_error_t plcrash_async_cfe_reader_init (plcrash_async_cfe_reader_t *reader, plcrash_async_mobject_t *mobj) {
    reader->mobj = mobj;

    /* Fetch and verify the header */
    struct unwind_info_section_header *header = plcrash_async_mobject_remap_address(mobj, plcrash_async_mobject_base_address(mobj), sizeof(*header));
    // TODO - byte order. CFE assumes host byte order
    if (header->version != 1) {
        // TODO
        PLCF_DEBUG("Unsupported CFE version: %" PRIu32, header->version);
        return PLCRASH_ENOTSUP;
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