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
 * A DWARF frame reader instance. Performs DWARF eh_frame/debug_frame parsing from a backing memory object.
 */
typedef struct plcrash_async_dwarf_frame_reader {
    /** A memory object containing the DWARF data at the starting address. */
    plcrash_async_mobject_t *mobj;

    /** The byte order of the encoded data. */
    const plcrash_async_byteorder_t *byteorder;
} plcrash_async_dwarf_frame_reader_t;


/**
 * DWARF Frame Descriptor Entry.
 */
typedef struct plcrash_async_dwarf_fde {
} plcrash_async_dwarf_fde_t;


plcrash_error_t plcrash_async_dwarf_frame_reader_init (plcrash_async_dwarf_frame_reader_t *reader, plcrash_async_mobject_t *mobj, const plcrash_async_byteorder_t *byteorder);

plcrash_error_t plcrash_async_dwarf_frame_reader_find_fde (plcrash_async_dwarf_frame_reader_t *reader, pl_vm_size_t offset, pl_vm_address_t pc);

void plcrash_async_dwarf_frame_reader_free (plcrash_async_dwarf_frame_reader_t *reader);


/**
 * @}
 */

#endif /* PLCRASH_ASYNC_DWARF_ENCODING_H */