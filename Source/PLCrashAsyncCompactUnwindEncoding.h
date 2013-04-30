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

#ifndef PLCRASH_ASYNC_COMPACT_UNWIND_ENCODING_H
#define PLCRASH_ASYNC_COMPACT_UNWIND_ENCODING_H 1

#include "PLCrashAsync.h"
#include "PLCrashAsyncImageList.h"

#include <mach-o/compact_unwind_encoding.h>

/**
 * @internal
 * @ingroup plcrash_async_cfe
 * @{
 */

/**
 * A CFE reader instance. Performs CFE data parsing from a backing memory object.
 */
typedef struct plcrash_async_cfe_reader {
    /** A memory object containing the CFE data at the starting address. */
    plcrash_async_mobject_t *mobj;

    /** The target CPU type. */
    cpu_type_t cputype;

    /** The unwind info header. Note that the header values may require byte-swapping for the local process' use. */
    struct unwind_info_section_header header;

    /** The byte order of the encoded data (including the header). */
    const plcrash_async_byteorder_t *byteorder;
} plcrash_async_cfe_reader_t;

/**
 * Supported CFE entry formats.
 */
typedef enum {
    /**
     * The frame pointer (fp) is valid. To walk the stack, the previous frame pointer may be popped from
     * the current frame pointer, followed by the return address.
     *
     * All non-volatile registers that need to be restored will be saved on the stack, ranging from fp±regsize through
     * fp±1020. The actual direction depends on the stack growth direction of the target platform.
     */
    PLCRASH_ASYNC_CFE_ENTRY_TYPE_FRAME_PTR = 1,

    /**
     * The frame pointer (eg, ebp/rbp) is invalid, but the stack size is constant and is small enough (<= 1024) that it
     * may be encoded in the CFE entry itself.
     *
     * The return address may be found at the provided ± offset from the stack pointer, followed all non-volatile
     * registers that need to be restored. The actual direction of the offset epends on the stack growth direction of
     * the target platform.
     */
    PLCRASH_ASYNC_CFE_ENTRY_TYPE_FRAMELESS_IMMD = 2,

    /**
     * The frame pointer (eg, ebp/rbp) is invalid, but the stack size is constant and is too large (>= 1024) to be
     * encoded in the CFE entry itself. Instead, the fixed stack size value must be extracted from an actual instruction
     * (eg, subl) within the target function, and used as the constant stack size. The decoded stack offset may be
     * added to the start address of the function to determine the location of the actual stack size.
     *
     * The return address may be found at the derived ± offset from the stack pointer, followed all non-volatile
     * registers that need to be restored. The actual direction of the offset epends on the stack growth direction of
     * the target platform.
     */
    PLCRASH_ASYNC_CFE_ENTRY_TYPE_FRAMELESS_INDIRECT = 3,

    /**
     * The unwinding information for the target address could not be encoded using the CFE format. Instead, DWARF
     * frame information must be used.
     *
     * An offset to the DWARF FDE in the __eh_frame section is be provided.
     */
    PLCRASH_ASYNC_CFE_ENTRY_TYPE_DWARF = 4
} plcrash_async_cfe_entry_type_t;

/**
 * A decoded CFE entry. The entry represents the data necessary to unwind the stack frame at a given PC, including
 * restoration of saved registers.
 */
typedef struct plcrash_async_cfe_entry {
    /** The CFE entry type. */
    plcrash_async_cfe_entry_type_t type;
} plcrash_async_cfe_entry_t;

plcrash_error_t plcrash_async_cfe_reader_init (plcrash_async_cfe_reader_t *reader, plcrash_async_mobject_t *mobj, cpu_type_t cputype);

plcrash_error_t plcrash_async_cfe_reader_find_pc (plcrash_async_cfe_reader_t *reader, pl_vm_address_t pc, uint32_t *encoding);

void plcrash_async_cfe_reader_free (plcrash_async_cfe_reader_t *reader);

/**
 * @} plcrash_async_cfe
 */

#endif /* PLCRASH_ASYNC_COMPACT_UNWIND_ENCODING_H */