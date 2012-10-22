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
#include <mach-o/nlist.h>

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

static uint64_t macho_swap64 (uint64_t input) {
    return OSSwapInt32(input);
}

static uint64_t macho_nswap64(uint64_t input) {
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
        /* NOTE: The image struct must be fully initialized before returning here, as otherwise our _free() function
         * will crash */
        return PLCRASH_EINTERNAL;
    }
    
    /* Set the default swap implementations */
    image->swap32 = macho_nswap32;
    image->swap64 = macho_nswap64;

    /* Parse the Mach-O magic identifier. */
    switch (image->header.magic) {
        case MH_CIGAM:
            // Enable byte swapping
            image->swap32 = macho_swap32;
            image->swap64 = macho_swap64;
            // Fall-through

        case MH_MAGIC:
            image->m64 = false;
            break;            
            
        case MH_CIGAM_64:
            // Enable byte swapping
            image->swap32 = macho_swap32;
            image->swap64 = macho_swap64;
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
 * Find the first LC_CMD matching the given @a cmd type, and optionally copy @a size bytes of the command to @a result.
 *
 * @param image The image to search.
 * @param cmd The LC_CMD type to find.
 * @param result A destination buffer to which @a size bytes of the command will be written, or NULL.
 * @param size The number of bytes to be read from the command and written to @a size.
 *
 * @return Returns the address of the matching load_command on success, or 0 on failure.
 */
pl_vm_address_t pl_async_macho_find_command (pl_async_macho_t *image, uint32_t cmd, void *result, pl_vm_size_t size) {
    pl_vm_address_t cmd_addr = 0;

    /* Even if no result was requested by the user, we still need to read the command ourselves. Configure a
     * buffer to hold the read command. */
    struct load_command cmdbuf;
    if (result == NULL) {
        result = &cmdbuf;
        size = sizeof(cmdbuf);
    }
    
    /* Iterate commands until we either find a match, or reach the end */
    while ((cmd_addr = pl_async_macho_next_command(image, cmd_addr)) != 0) {        
        kern_return_t kt;
        if ((kt = plcrash_async_read_addr(image->task, cmd_addr, result, size)) != KERN_SUCCESS) {
            PLCF_DEBUG("Failed to read LC_CMD at address %" PRIu64 " with error %d in: %s", (uint64_t) cmd_addr, kt, image->name);
            return 0;
        }

        /* Return a match */
        struct load_command *r = result;
        if (image->swap32(r->cmd) == cmd) {
            return cmd_addr;
        }
    }
    
    /* No match found */
    return 0;
}

/**
 * Find and map a named segment, initializing @a mobj. It is the caller's responsibility to dealloc @a mobj after
 * a successful initialization
 *
 * @param image The image to search for @a segname.
 * @param segname The name of the segment to be mapped.
 * @param mobj The mobject to be initialized with a mapping of the segment's data. It is the caller's responsibility to dealloc @a mobj after
 * a successful initialization.
 *
 * @return Returns PLCRASH_ESUCCESS on success, or an error result on failure. 
 */
plcrash_error_t pl_async_macho_map_segment (pl_async_macho_t *image, const char *segname, plcrash_async_mobject_t *mobj) {
    pl_vm_address_t cmd_addr = 0;

    // bool found_segment = false;
    uint32_t cmdsize = 0;

    while ((cmd_addr = pl_async_macho_next_command_type(image, cmd_addr, image->m64 ? LC_SEGMENT_64 : LC_SEGMENT, &cmdsize)) != 0) {
        /* Read the load command */
        struct segment_command cmd_32;
        struct segment_command_64 cmd_64;

        /* Read in the full segment command */
        plcrash_error_t err;
        if (image->m64)
            err = plcrash_async_read_addr(image->task, cmd_addr, &cmd_64, sizeof(cmd_64));
        else
            err = plcrash_async_read_addr(image->task, cmd_addr, &cmd_32, sizeof(cmd_32));
        
        if (err != PLCRASH_ESUCCESS)
            return err;
        
        /* Check for match */
        if (plcrash_async_strcmp(segname, image->m64 ? cmd_64.segname : cmd_32.segname) != 0)
            continue;

        /* Calculate the in-memory address and size */
        pl_vm_address_t segaddr;
        pl_vm_size_t segsize;
        if (image->m64) {
            segaddr = image->swap64(cmd_64.vmaddr) + image->vmaddr_slide;
            segsize = image->swap64(cmd_64.vmsize);
        } else {
            segaddr = image->swap32(cmd_32.vmaddr) + image->vmaddr_slide;
            segsize = image->swap32(cmd_32.vmsize);
        }

        /* Perform and return the mapping */
        return plcrash_async_mobject_init(mobj, image->task, segaddr, segsize);
    }

    /* If the loop terminates, the segment was not found */
    return PLCRASH_ENOTFOUND;
}

plcrash_error_t pl_async_macho_find_symbol (pl_async_macho_t *image, pl_vm_address_t pc) {
    struct symtab_command symtab_cmd;
    struct dysymtab_command dysymtab_cmd;
    kern_return_t kt;

    /* Fetch the symtab commands, if available. */
    pl_vm_address_t symtab_cmd_addr = pl_async_macho_find_command(image, LC_SYMTAB, &symtab_cmd, sizeof(symtab_cmd));
    pl_vm_address_t dysymtab_cmd_addr = pl_async_macho_find_command(image, LC_DYSYMTAB, &dysymtab_cmd, sizeof(dysymtab_cmd));

    /* The symtab command is required */
    if (symtab_cmd_addr == 0) {
        PLCF_DEBUG("could not find LC_SYMTAB load command");
        return PLCRASH_ENOTFOUND;
    }

    /* Read in the symtab command */
    if ((kt = plcrash_async_read_addr(image->task, symtab_cmd_addr, &symtab_cmd, sizeof(symtab_cmd))) != KERN_SUCCESS) {
        PLCF_DEBUG("plcrash_async_read_addr() failure: %d", kt);
        return PLCRASH_EINVAL;
    }
    
    /* If available, read in the dsymtab_cmd */
    if (dysymtab_cmd_addr != 0) {
        if ((kt = plcrash_async_read_addr(image->task, dysymtab_cmd_addr, &dysymtab_cmd, sizeof(dysymtab_cmd))) != KERN_SUCCESS) {
            PLCF_DEBUG("plcrash_async_read_addr() failure: %d", kt);
            return PLCRASH_EINVAL;
        }
    }

    /* Map in the __LINKEDIT segment, which includes the symbol and string tables */
    // TODO - This will leak!
    plcrash_async_mobject_t linkedit_mobj;
    plcrash_error_t err = pl_async_macho_map_segment(image, "__LINKEDIT", &linkedit_mobj);
    if (err != PLCRASH_ESUCCESS) {
        PLCF_DEBUG("plcrash_async_mobject_init() failure: %d", err);
        return PLCRASH_EINTERNAL;
    }

    /* Determine the string and symbol table sizes. */
    size_t nsyms = image->swap32(symtab_cmd.nsyms);
    size_t nlist_size = nsyms * (image->m64 ? sizeof(struct nlist_64) : sizeof(struct nlist));

    size_t string_size = image->swap32(symtab_cmd.strsize);

    /* Fetch pointers to the symbol and string tables, and verify their size values */
    void *nlist_table;
    char *string_table;

    uintptr_t remapped_addr = plcrash_async_mobject_remap_address(&linkedit_mobj, image->header_addr + image->swap32(symtab_cmd.symoff));
    nlist_table = plcrash_async_mobject_pointer(&linkedit_mobj, remapped_addr, nlist_size);    
    if (nlist_table == NULL) {
        PLCF_DEBUG("plcrash_async_mobject_pointer(mobj, %" PRIx64 ", %" PRIx64") returned NULL", (uint64_t) linkedit_mobj.address + image->swap32(symtab_cmd.symoff), (uint64_t) nlist_size);
        return PLCRASH_EINTERNAL;
    }

    remapped_addr = plcrash_async_mobject_remap_address(&linkedit_mobj, image->header_addr + image->swap32(symtab_cmd.stroff));
    string_table = plcrash_async_mobject_pointer(&linkedit_mobj, remapped_addr, string_size);
    if (string_table == NULL) {
        PLCF_DEBUG("plcrash_async_mobject_pointer(mobj, %" PRIx64 ", %" PRIx64") returned NULL", (uint64_t) linkedit_mobj.address + image->swap32(symtab_cmd.stroff), (uint64_t) string_size);
        return PLCRASH_EINTERNAL;
    }

    if (image->m64) {
        struct nlist_64 *symbols = nlist_table;
        PLCF_DEBUG("Total count=%d", (int)nsyms);
        
        // TODO: We know that symbols[i] is valid, since we fetched a pointer+len based on the value using
        // plcrash_async_mobject_pointer() above.
        //
        // What we don't know is whether n_strx index is valid. The safest way to handle that is probably
        // to use a combination of plcrash_async_mobject_pointer and a strncpy() implementation to prevent
        // walking past the end of the valid range.
        //
        // Additionally, we're going to need somewhere semi-persistent to store the symbol information,
        // as the __LINKEDIT mapping must be discarded when this function exits.
        for (uint32_t i = 0; i < nsyms; i++) {
            if ((symbols[i].n_type & N_TYPE) != N_SECT || ((symbols[i].n_type & N_STAB) != 0))
                continue;

            PLCF_DEBUG("FOUND SYM %s", string_table + symbols[i].n_un.n_strx);
        }
    }

    // TODO
    return PLCRASH_ESUCCESS;
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