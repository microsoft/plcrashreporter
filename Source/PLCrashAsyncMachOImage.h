/*
 * Author: Landon Fuller <landonf@plausible.coop>
 *
 * Copyright (c) 2011-2012 Plausible Labs Cooperative, Inc.
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

#include <stdint.h>
#include <mach/mach.h>
#include <mach-o/loader.h>

#include "PLCrashAsyncMObject.h"

/**
 * @internal
 * @ingroup plcrash_async_image
 * @{
 */

/**
 * A Mach-O image instance.
 */
typedef struct pl_async_macho {    
    /** The Mach task in which the Mach-O image can be found */
    mach_port_t task;

    /** The binary image's header address. */
    pl_vm_address_t header_addr;
    
    /** The binary's dyld-reported reported vmaddr slide. */
    int64_t vmaddr_slide;

    /** The binary image's name/path. */
    char *name;

    /** The Mach-O header. For our purposes, the 32-bit and 64-bit headers are identical. Note that the header
     * values may require byte-swapping for the local process' use. */
    struct mach_header header;
    
    /** Total size, in bytes, of the in-memory Mach-O header. This may differ from the header field above,
     * as the above field does not include the full mach_header_64 extensions to the mach_header. */
    pl_vm_size_t header_size;

    /** Total size, in bytes, of the Mach-O image's __TEXT segment. */
    pl_vm_size_t text_size;

    /** If true, the image is 64-bit Mach-O. If false, it is a 32-bit Mach-O image. */
    bool m64;

    /** The byte-swap function to use for 16-bit values. */
    uint16_t (*swap16)(uint16_t);

    /** The byte-swap function to use for 32-bit values. */
    uint32_t (*swap32)(uint32_t);
    
    /** The byte-swap function to use for 64-bit values. */
    uint64_t (*swap64)(uint64_t);
} pl_async_macho_t;

/**
 * Prototype of a callback function used to execute user code with async-safely fetched symbol.
 *
 * @param address The symbol address.
 * @param name The symbol name. The callback is responsible for copying this value, as its backing storage is not gauranteed to exist
 * after the callback returns.
 * @param context The API client's supplied context value.
 */
typedef void (*pl_async_macho_found_symbol_cb)(pl_vm_address_t address, const char *name, void *ctx);

plcrash_error_t pl_async_macho_init (pl_async_macho_t *image, mach_port_t task, const char *name, pl_vm_address_t header, int64_t vmaddr_slide);

pl_vm_address_t pl_async_macho_next_command (pl_async_macho_t *image, pl_vm_address_t previous);
pl_vm_address_t pl_async_macho_next_command_type (pl_async_macho_t *image, pl_vm_address_t previous, uint32_t expectedCommand, uint32_t *size);
pl_vm_address_t pl_async_macho_find_command (pl_async_macho_t *image, uint32_t cmd, void *result, pl_vm_size_t size);

plcrash_error_t pl_async_macho_find_segment (pl_async_macho_t *image, const char *segname, pl_vm_address_t *outAddress, struct segment_command *outCmd_32, struct segment_command_64 *outCmd_64);
plcrash_error_t pl_async_macho_map_segment (pl_async_macho_t *image, const char *segname, plcrash_async_mobject_t *mobj);

plcrash_error_t pl_async_macho_map_section (pl_async_macho_t *image, const char *segname, const char *sectname, plcrash_async_mobject_t *mobj);

plcrash_error_t pl_async_macho_find_symbol (pl_async_macho_t *image, pl_vm_address_t pc, pl_async_macho_found_symbol_cb symbol_cb, void *context);

void pl_async_macho_free (pl_async_macho_t *image);

/**
 * @}
 */