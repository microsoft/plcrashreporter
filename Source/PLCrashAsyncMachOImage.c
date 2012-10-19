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

#include "PLCrashAsyncMachOImage.h"

#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <mach-o/fat.h>

/**
 * @internal
 * @ingroup plcrash_async
 * @defgroup plcrash_async_image Binary Image Parsing
 *
 * Implements async-safe Mach-O binary parsing, for use at crash time when extracting binary information
 * from the crashed process.
 * @{
 */

/* Simple byteswap wrappers */
static uint32_t macho_swap32 (uint32_t input) {
    return OSSwapInt32(input);
}

static uint32_t macho_nswap32(uint32_t input) {
    return input;
}

/**
 * Initialize a new Mach-O binary image parser.
 *
 * @param image The image structure to be initialized.
 * @param name The file name or path for the Mach-O image.
 * @param header The task-local address of the image's Mach-O header.
 * @param vmaddr_slide The dyld-reported task-local vmaddr slide of this image.
 *
 * @return PLCRASH_ESUCCESS on success. PLCRASH_EINVAL will be returned in the Mach-O file can not be parsed,
 * or PLCRASH_EINTERNAL if an error occurs reading from the target task.
 *
 * @warning This method is not async safe.
 * @note On error, pl_async_macho_free() must be called to free any resources still held by the @a image.
 */
plcrash_error_t pl_async_macho_init (pl_async_macho_t *image, mach_port_t task, const char *name, pl_vm_address_t header, int64_t vmaddr_slide) {
    /* This must be done first, as our free() function will always decrement the port's reference count. */
    mach_port_mod_refs(mach_task_self(), task, MACH_PORT_RIGHT_SEND, 1);
    image->task = task;

    image->header_addr = header;
    image->vmaddr_slide = vmaddr_slide;
    image->name = strdup(name);

    /* Read in the Mach-O header */
    if (plcrash_async_read_addr(image->task, image->header_addr, &image->header, sizeof(image->header)) != KERN_SUCCESS) {
        return PLCRASH_EINTERNAL;
    }
    
    /* Set the default swap implementations */
    image->swap32 = macho_nswap32;

    /* Parse the Mach-O magic identifier. */
    switch (image->header.magic) {
        case MH_CIGAM:
            // Enable byte swapping
            image->swap32 = macho_swap32;

            // Fall-through

        case MH_MAGIC:
            image->m64 = false;
            break;            
            
        case MH_CIGAM_64:
            // Enable byte swapping
            image->swap32 = macho_swap32;
            // Fall-through
            
        case MH_MAGIC_64:
            image->m64 = true;
            break;

        case FAT_CIGAM:
        case FAT_MAGIC:
            PLCF_DEBUG("%s called with an unsupported universal Mach-O archive in: %s", __func__, image->name);
            return PLCRASH_EINVAL;
            break;

        default:
            PLCF_DEBUG("Unknown Mach-O magic: 0x%" PRIx32 " in: %s", image->header.magic, image->name);
            return PLCRASH_EINVAL;
    }

    /* Save the header size */
    if (image->m64) {
        image->header_size = sizeof(struct mach_header_64);
    } else {
        image->header_size = sizeof(struct mach_header);
    }

    return PLCRASH_ESUCCESS;
}

/**
 * Iterate over the available Mach-O LC_CMD entries.
 *
 * @param image The image to iterate
 * @param previous The previously returned LC_CMD address value, or 0 to iterate from the first LC_CMD.
 * @return Returns the address of the next load_command on success, or 0 on failure.
 */
pl_vm_address_t pl_async_macho_next_command (pl_async_macho_t *image, pl_vm_address_t previous) {
    struct load_command cmd;

    /* On the first iteration, determine the LC_CMD offset from the Mach-O header. */
    if (previous == 0) {
        previous = image->header_addr + image->header_size;
    }

    /* Read the previous command value to get its size. */
    if (plcrash_async_read_addr(image->task, previous, &cmd, sizeof(cmd)) != KERN_SUCCESS) {
        PLCF_DEBUG("Failed to read LC_CMD at address %" PRIu64 " in: %s", (uint64_t) previous, image->name);
        return 0;
    }
    
    /* Fetch the size */
    uint32_t cmdsize = image->swap32(cmd.cmdsize);
    
    /* Verify that incrementing our command position won't overflow our address value */
    if (PL_VM_ADDRESS_MAX - cmdsize < previous) {
        PLCF_DEBUG("Received an unexpectedly large cmdsize value: %" PRIu32 " in: %s", cmdsize, image->name);
        return 0;
    }

    /* Advance to the next command */
    pl_vm_address_t next = previous + cmdsize;
    if (next >= image->header_addr + image->swap32(image->header.sizeofcmds)) {
        PLCF_DEBUG("Reached the LC_CMD end in: %s", image->name);
        return 0;
    }

    return next;
}

/**
 * Iterate over the available Mach-O LC_CMD entries.
 *
 * @param image The image to iterate
 * @param previous The previously returned LC_CMD address value, or 0 to iterate from the first LC_CMD.
 * @param expectedCommand The LC_* command type to be returned. Only commands matching this type will be returned by the iterator.
 * @return Returns the address of the next load_command on success, or 0 on failure. If @a size is non-NULL, the size of
 * the next load command will be written to @a size.
 */
pl_vm_address_t pl_async_macho_next_command_type (pl_async_macho_t *image, pl_vm_address_t previous, uint32_t expectedCommand, uint32_t *size) {
    pl_vm_address_t cmd_addr = previous;

    /* Iterate commands until we either find a match, or reach the end */
    while ((cmd_addr = pl_async_macho_next_command(image, cmd_addr)) != 0) {
        /* Read the load command type */
        struct load_command cmd;
        
        kern_return_t kt;
        if ((kt = plcrash_async_read_addr(image->task, cmd_addr, &cmd, sizeof(cmd))) != KERN_SUCCESS) {
            PLCF_DEBUG("Failed to read LC_CMD at address %" PRIu64 " with error %d in: %s", (uint64_t) cmd_addr, kt, image->name);
            return 0;
        }

        /* Return a match */
        if (image->swap32(cmd.cmd) == expectedCommand) {
            if (size != NULL)
                *size = image->swap32(cmd.cmdsize);
            return cmd_addr;
        }
    }

    /* No match found */
    return 0;
}

/**
 * Free all Mach-O binary image resources.
 *
 * @warning This method is not async safe.
 */
void pl_async_macho_free (pl_async_macho_t *image) {
    if (image->name != NULL)
        free(image->name);

    mach_port_mod_refs(mach_task_self(), image->task, MACH_PORT_RIGHT_SEND, -1);
}


/**
 * @} pl_async_macho
 */