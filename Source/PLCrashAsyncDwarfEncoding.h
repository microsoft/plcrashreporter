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
    
    /** True if this is a debug_frame section */
    bool debug_frame;
} plcrash_async_dwarf_frame_reader_t;


/**
 * DWARF Frame Descriptor Entry.
 */
typedef struct plcrash_async_dwarf_fde_info {
    /** The starting address of the entry (not including the initial length field),
     * relative to the eh_frame/debug_frame section base. */
    pl_vm_address_t fde_offset;

    /** The FDE entry length, not including the initial length field. */
    uint64_t fde_length;
    
    /** Offset to the CIE associated with this FDE, relative to the eh_frame/debug_frame section base. */
    pl_vm_address_t cie_offset;

    /** The address of the FDE instructions, relative to the eh_frame/debug_frame section base. */
    pl_vm_address_t fde_instruction_offset;

    /** The start of the IP range covered by this FDE. The address is relative to the image's base address, <em>not</em>
     * the in-memory PC address of a loaded images. */
    uint64_t pc_start;

    /** The end of the IP range covered by this FDE. The address is relative to the image's base address, <em>not</em>
     * the in-memory PC address of a loaded images.  */
    uint64_t pc_end;
} plcrash_async_dwarf_fde_info_t;


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


/**
 * @}
 */

#endif /* PLCRASH_ASYNC_DWARF_ENCODING_H */