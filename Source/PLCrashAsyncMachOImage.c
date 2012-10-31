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
#include <assert.h>

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
static uint16_t macho_swap16 (uint16_t input) {
    return OSSwapInt16(input);
}

static uint16_t macho_nswap16 (uint16_t input) {
    return input;
}

static uint32_t macho_swap32 (uint32_t input) {
    return OSSwapInt32(input);
}

static uint32_t macho_nswap32 (uint32_t input) {
    return input;
}

static uint64_t macho_swap64 (uint64_t input) {
    return OSSwapInt64(input);
}

static uint64_t macho_nswap64 (uint64_t input) {
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
    kern_return_t kt;
    if ((kt = plcrash_async_read_addr(image->task, image->header_addr, &image->header, sizeof(image->header))) != KERN_SUCCESS) {
        /* NOTE: The image struct must be fully initialized before returning here, as otherwise our _free() function
         * will crash */
        PLCF_DEBUG("Failed to read Mach-O header from 0x%" PRIx64 " for image %s, kern_error=%d", (uint64_t) image->header_addr, name, kt);
        return PLCRASH_EINTERNAL;
    }
    
    /* Set the default swap implementations */
    image->swap16 = macho_nswap16;
    image->swap32 = macho_nswap32;
    image->swap64 = macho_nswap64;

    /* Parse the Mach-O magic identifier. */
    switch (image->header.magic) {
        case MH_CIGAM:
            // Enable byte swapping
            image->swap16 = macho_swap16;
            image->swap32 = macho_swap32;
            image->swap64 = macho_swap64;
            // Fall-through

        case MH_MAGIC:
            image->m64 = false;
            break;            
            
        case MH_CIGAM_64:
            // Enable byte swapping
            image->swap16 = macho_swap16;
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

    /* Now that the image has been sufficiently initialized, determine the __TEXT segment size */
    pl_vm_address_t cmd_addr = 0;
    image->text_size = 0x0;
    bool found_text_seg = false;
    while ((cmd_addr = pl_async_macho_next_command_type(image, cmd_addr, image->m64 ? LC_SEGMENT_64 : LC_SEGMENT, NULL)) != 0) {
        if (image->m64) {
            struct segment_command_64 segment;
            if (plcrash_async_read_addr(image->task, cmd_addr, &segment, sizeof(segment)) != KERN_SUCCESS) {
                PLCF_DEBUG("Failed to read LC_SEGMENT command");
                return PLCRASH_EINVAL;
            }
            
            if (plcrash_async_strncmp(segment.segname, SEG_TEXT, sizeof(segment.segname)) != 0)
                continue;
            
            image->text_size = image->swap64(segment.vmsize);
            found_text_seg = true;
            break;
        } else {
            struct segment_command segment;
            if (plcrash_async_read_addr(image->task, cmd_addr, &segment, sizeof(segment)) != KERN_SUCCESS) {
                PLCF_DEBUG("Failed to read LC_SEGMENT command");
                return PLCRASH_EINVAL;
            }
            
            if (plcrash_async_strncmp(segment.segname, SEG_TEXT, sizeof(segment.segname)) != 0)
                continue;
            
            image->text_size = image->swap32(segment.vmsize);
            found_text_seg = true;
            break;
        }
    }

    if (!found_text_seg) {
        PLCF_DEBUG("Could not find __TEXT segment!");
        return PLCRASH_EINVAL;
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
        /* Sanity check */
        if (image->swap32(image->header.sizeofcmds) < sizeof(struct load_command)) {
            PLCF_DEBUG("Mach-O sizeofcmds is less than sizeof(struct load_command) in %s", image->name);
            return 0;
        }

        return image->header_addr + image->header_size;
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
 * Find a named segment.
 *
 * @param image The image to search for @a segname.
 * @param segname The name of the segment to search for.
 * @param outAddress On successful return, contains the address of the found segment.
 * @param outCmd_32 On successful return with a 32-bit image, contains the segment header.
 * @param outCmd_64 On successful return with a 64-bit image, contains the segment header.
 *
 * @return Returns PLCRASH_ESUCCESS on success, or an error result on failure.
 */
plcrash_error_t pl_async_macho_find_segment (pl_async_macho_t *image, const char *segname, pl_vm_address_t *outAddress, struct segment_command *outCmd_32, struct segment_command_64 *outCmd_64) {
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
        if (plcrash_async_strncmp(segname, image->m64 ? cmd_64.segname : cmd_32.segname, sizeof(cmd_64.segname)) == 0) {
            *outAddress = cmd_addr;
            if (outCmd_32 != NULL)
                *outCmd_32 = cmd_32;
            if (outCmd_64 != NULL)
                *outCmd_64 = cmd_64;
            return PLCRASH_ESUCCESS;
        }
    }
    
    return PLCRASH_ENOTFOUND;
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
    struct segment_command cmd_32;
    struct segment_command_64 cmd_64;
    
    plcrash_error_t err = pl_async_macho_find_segment(image, segname, &cmd_addr, &cmd_32, &cmd_64);
    if (err != PLCRASH_ESUCCESS)
        return err;
    
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

/**
 * Find and map a named section within a named segment, initializing @a mobj.
 * It is the caller's responsibility to dealloc @a mobj after a successful
 * initialization
 *
 * @param image The image to search for @a segname.
 * @param segname The name of the segment to search.
 * @param sectname The name of the section to map.
 * @param mobj The mobject to be initialized with a mapping of the section's data. It is the caller's responsibility to dealloc @a mobj after
 * a successful initialization.
 *
 * @return Returns PLCRASH_ESUCCESS on success, or an error result on failure.
 */
plcrash_error_t pl_async_macho_map_section (pl_async_macho_t *image, const char *segname, const char *sectname, plcrash_async_mobject_t *mobj) {
    pl_vm_address_t cmd_addr = 0;
    struct segment_command cmd_32;
    struct segment_command_64 cmd_64;
    
    plcrash_error_t err = pl_async_macho_find_segment(image, segname, &cmd_addr, &cmd_32, &cmd_64);
    if (err != PLCRASH_ESUCCESS)
        return err;
    
    uint32_t nsects;
    pl_vm_address_t cursor = cmd_addr;
    
    if (image->m64) {
        nsects = cmd_64.nsects;
        cursor += sizeof(cmd_64);
    } else {
        nsects = cmd_32.nsects;
        cursor += sizeof(cmd_32);
    }
    
    for (uint32_t i = 0; i < nsects; i++) {
        struct section sect_32;
        struct section_64 sect_64;
        
        if (image->m64) {
            err = plcrash_async_read_addr(image->task, cursor, &sect_64, sizeof(sect_64));
            cursor += sizeof(sect_64);
        } else {
            err = plcrash_async_read_addr(image->task, cursor, &sect_32, sizeof(sect_32));
            cursor += sizeof(sect_32);
        }
        
        if (err != PLCRASH_ESUCCESS)
            return err;
        
        const char *image_sectname = image->m64 ? sect_64.sectname : sect_32.sectname;
        if (plcrash_async_strncmp(sectname, image_sectname, sizeof(sect_64.sectname)) == 0) {
            /* Calculate the in-memory address and size */
            pl_vm_address_t sectaddr;
            pl_vm_size_t sectsize;
            if (image->m64) {
                sectaddr = image->swap64(sect_64.addr) + image->vmaddr_slide;
                sectsize = image->swap32(sect_64.size);
            } else {
                sectaddr = image->swap32(sect_32.addr) + image->vmaddr_slide;
                sectsize = image->swap32(sect_32.size);
            }
            
            
            /* Perform and return the mapping */
            return plcrash_async_mobject_init(mobj, image->task, sectaddr, sectsize);
        }
    }
    
    return PLCRASH_ENOTFOUND;
}

/**
 * @internal
 * Common wrapper of nlist/nlist_64. We verify that this union is valid for our purposes in pl_async_macho_find_symtab_symbol().
 */
typedef union {
    struct nlist_64 n64;
    struct nlist n32;
} pl_nlist_common;


/*
 * Locate a symtab entry for @a slide_pc within @a symbtab. This is performed using best-guess heuristics, and may
 * be incorrect.
 *
 * @param image The Mach-O image to search for @a pc
 * @param slide_pc The PC value within the target process for which symbol information should be found. The VM slide
 * address should have already been applied to this value.
 * @param symtab The symtab to search.
 * @param nsyms The number of nlist entries available via @a symtab.
 * @param found_symbol A reference to the previous best match symbol. The referenced value should be initialized
 * to NULL before calling this function for the first time. Future invocations will reference this value to determine
 * whether a closer match has been found.
 *
 * @return Returns true if a symbol was found, false otherwise.
 *
 * @warning This function implements no validation of the symbol table pointer. It is the caller's responsibility
 * to verify that the referenced symtab is valid and mapped PROT_READ.
 */
static bool pl_async_macho_find_symtab_symbol (pl_async_macho_t *image, pl_vm_address_t slide_pc, pl_nlist_common *symtab,
                                               uint32_t nsyms, pl_nlist_common **found_symbol)
{
    /* nlist_64 and nlist are identical other than the trailing address field, so we use
     * a union to share a common implementation of symbol lookup. The following asserts
     * provide a sanity-check of that assumption, in the case where this code is moved
     * to a new platform ABI. */
    {
#define pl_m_sizeof(type, field) sizeof(((type *)NULL)->field)
        
        PLCF_ASSERT(__offsetof(struct nlist_64, n_type) == __offsetof(struct nlist, n_type));
        PLCF_ASSERT(pl_m_sizeof(struct nlist_64, n_type) == pl_m_sizeof(struct nlist, n_type));
        
        PLCF_ASSERT(__offsetof(struct nlist_64, n_un.n_strx) == __offsetof(struct nlist, n_un.n_strx));
        PLCF_ASSERT(pl_m_sizeof(struct nlist_64, n_un.n_strx) == pl_m_sizeof(struct nlist, n_un.n_strx));
        
        PLCF_ASSERT(__offsetof(struct nlist_64, n_value) == __offsetof(struct nlist, n_value));
        
#undef pl_m_sizeof
    }
    
#define pl_sym_value(nl) (image->m64 ? image->swap64((nl)->n64.n_value) : image->swap32((nl)->n32.n_value))
    
    /* Walk the symbol table. We know that symbols[i] is valid, since we fetched a pointer+len based on the value using
     * plcrash_async_mobject_pointer() above. */
    for (uint32_t i = 0; i < nsyms; i++) {
        /* Perform 32-bit/64-bit dependent aliased pointer math. */
        pl_nlist_common *symbol;
        if (image->m64) {
            symbol = (pl_nlist_common *) &(((struct nlist_64 *) symtab)[i]);
        } else {
            symbol = (pl_nlist_common *) &(((struct nlist *) symtab)[i]);
        }
        
        /* Symbol must be within a section, and must not be a debugging entry. */
        if ((symbol->n32.n_type & N_TYPE) != N_SECT || ((symbol->n32.n_type & N_STAB) != 0))
            continue;
        
        /* Search for the best match. We're looking for the closest symbol occuring before PC. */
        if (pl_sym_value(symbol) <= slide_pc && (*found_symbol == NULL || pl_sym_value(*found_symbol) < pl_sym_value(symbol))) {
            *found_symbol = symbol;
        }
    }
    
    /* If no symbol was found (unlikely!), return not found */
    if (*found_symbol == NULL) {
        return false;
    }

    return true;
}

/**
 * Attempt to locate a symbol address and name for @a pc within @a image. This is performed using best-guess heuristics, and may
 * be incorrect.
 *
 * @param image The Mach-O image to search for @a pc
 * @param pc The PC value within the target process for which symbol information should be found.
 * @param symbol_cb A callback to be called if the symbol is found.
 * @param context Context to be passed to @a found_symbol.
 *
 * @return Returns PLCRASH_ESUCCESS if the symbol is found. If the symbol is not found, @a found_symbol will not be called.
 */
plcrash_error_t pl_async_macho_find_symbol (pl_async_macho_t *image, pl_vm_address_t pc, pl_async_macho_found_symbol_cb symbol_cb, void *context) {
    struct symtab_command symtab_cmd;
    struct dysymtab_command dysymtab_cmd;
    kern_return_t kt;
    plcrash_error_t retval;
    
    /* Compute the actual in-core PC. */
    pl_vm_address_t slide_pc = pc - image->vmaddr_slide;

    /* Fetch the symtab commands, if available. */
    pl_vm_address_t symtab_cmd_addr = pl_async_macho_find_command(image, LC_SYMTAB, &symtab_cmd, sizeof(symtab_cmd));
    pl_vm_address_t dysymtab_cmd_addr = pl_async_macho_find_command(image, LC_DYSYMTAB, &dysymtab_cmd, sizeof(dysymtab_cmd));
    
    PLCF_DEBUG("Searching for symbol in %s", image->name);

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
    plcrash_async_mobject_t linkedit_mobj;
    plcrash_error_t err = pl_async_macho_map_segment(image, "__LINKEDIT", &linkedit_mobj);
    if (err != PLCRASH_ESUCCESS) {
        PLCF_DEBUG("plcrash_async_mobject_init() failure: %d", err);
        return PLCRASH_EINTERNAL;
    }

    /* Determine the string and symbol table sizes. */
    uint32_t nsyms = image->swap32(symtab_cmd.nsyms);
    size_t nlist_struct_size = image->m64 ? sizeof(struct nlist_64) : sizeof(struct nlist);
    size_t nlist_table_size = nsyms * nlist_struct_size;

    size_t string_size = image->swap32(symtab_cmd.strsize);

    /* Fetch pointers to the symbol and string tables, and verify their size values */
    void *nlist_table;
    char *string_table;

    uintptr_t remapped_addr = plcrash_async_mobject_remap_address(&linkedit_mobj, image->header_addr + image->swap32(symtab_cmd.symoff));
    nlist_table = plcrash_async_mobject_pointer(&linkedit_mobj, remapped_addr, nlist_table_size);
    if (nlist_table == NULL) {
        PLCF_DEBUG("plcrash_async_mobject_pointer(mobj, %" PRIx64 ", %" PRIx64") returned NULL mapping __LINKEDIT.symoff", (uint64_t) linkedit_mobj.address + image->swap32(symtab_cmd.symoff), (uint64_t) nlist_table_size);
        retval = PLCRASH_EINTERNAL;
        goto cleanup;
    }

    remapped_addr = plcrash_async_mobject_remap_address(&linkedit_mobj, image->header_addr + image->swap32(symtab_cmd.stroff));
    string_table = plcrash_async_mobject_pointer(&linkedit_mobj, remapped_addr, string_size);
    if (string_table == NULL) {
        PLCF_DEBUG("plcrash_async_mobject_pointer(mobj, %" PRIx64 ", %" PRIx64") returned NULL mapping __LINKEDIT.stroff", (uint64_t) linkedit_mobj.address + image->swap32(symtab_cmd.stroff), (uint64_t) string_size);
        retval = PLCRASH_EINTERNAL;
        goto cleanup;
    }
            
    /* Walk the symbol table. We know that the full range of symbols[nsyms-1] is valid, since we fetched a pointer+len
     * based on the value using plcrash_async_mobject_pointer() above. */
    pl_nlist_common *found_symbol = NULL;

    if (dysymtab_cmd_addr != 0x0) {
        /* dysymtab is available; use it to constrain our symbol search to the global and local sections of the symbol table. */

        uint32_t idx_syms_global = image->swap32(dysymtab_cmd.iextdefsym);
        uint32_t idx_syms_local = image->swap32(dysymtab_cmd.ilocalsym);
        
        uint32_t nsyms_global = image->swap32(dysymtab_cmd.nextdefsym);
        uint32_t nsyms_local = image->swap32(dysymtab_cmd.nlocalsym);

        /* Sanity check the symbol offsets to ensure they're within our known-valid ranges */
        if (idx_syms_global + nsyms_global > nsyms || idx_syms_local + nsyms_local > nsyms) {
            PLCF_DEBUG("iextdefsym=%" PRIx32 ", ilocalsym=%" PRIx32 " out of range nsym=%" PRIx32, idx_syms_global+nsyms_global, idx_syms_local+nsyms_local, nsyms);
            retval = PLCRASH_EINVAL;
            goto cleanup;
        }

        pl_nlist_common *global_nlist;
        pl_nlist_common *local_nlist;
        if (image->m64) {
            struct nlist_64 *n64 = nlist_table;
            global_nlist = (pl_nlist_common *) (n64 + idx_syms_global);
            local_nlist = (pl_nlist_common *) (n64 + idx_syms_local);
        } else {
            struct nlist *n32 = nlist_table;
            global_nlist = (pl_nlist_common *) (n32 + idx_syms_global);
            local_nlist = (pl_nlist_common *) (n32 + idx_syms_local);
        }

        pl_async_macho_find_symtab_symbol(image, slide_pc, global_nlist, nsyms_global, &found_symbol);
        pl_async_macho_find_symtab_symbol(image, slide_pc, local_nlist, nsyms_local, &found_symbol);
    } else {
        /* If dysymtab is not available, search all symbols */
        pl_async_macho_find_symtab_symbol(image, slide_pc, nlist_table, nsyms, &found_symbol);
    }

    /* No symbol found. */
    if (found_symbol == NULL) {
        retval = PLCRASH_ENOTFOUND;
        goto cleanup;
    }

    /* Symbol found!
     *
     * It's possible, though unlikely, that the n_strx index value is invalid. To handle this,
     * we walk the string until \0 is hit, verifying that it can be found in its entirety within
     *
     * TODO: Evaluate effeciency of per-byte calling of plcrash_async_mobject_pointer().
     */
    const char *sym_name = string_table + image->swap32(found_symbol->n32.n_un.n_strx);
    const char *p = sym_name;
    do {
        if (plcrash_async_mobject_pointer(&linkedit_mobj, (uintptr_t) p, 1) == NULL) {
            retval = PLCRASH_EINVAL;
            goto cleanup;
        }
        p++;
    } while (*p != '\0');

    /* Determine the correct symbol address. We have to set the low-order bit ourselves for ARM THUMB functions. */
    vm_address_t sym_addr = 0x0;
    if (image->swap16(found_symbol->n32.n_desc) & N_ARM_THUMB_DEF)
        sym_addr = (pl_sym_value(found_symbol)|1) + image->vmaddr_slide;
    else
        sym_addr = pl_sym_value(found_symbol) + image->vmaddr_slide;

    /* Inform our caller */
    symbol_cb(sym_addr, sym_name, context);

    // fall through to cleanup
    retval = PLCRASH_ESUCCESS;

cleanup:
    plcrash_async_mobject_free(&linkedit_mobj);
    return retval;
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