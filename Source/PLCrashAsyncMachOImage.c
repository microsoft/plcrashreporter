/*
 * Author: Landon Fuller <landonf@plausible.coop>
 *
 * Copyright (c) 2011-2013 Plausible Labs Cooperative, Inc.
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
plcrash_error_t plcrash_nasync_macho_init (plcrash_async_macho_t *image, mach_port_t task, const char *name, pl_vm_address_t header, int64_t vmaddr_slide) {
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
    
    /* Map in load commands */
    /* Map in header + load commands */
    pl_vm_size_t cmd_len = image->swap32(image->header.sizeofcmds);
    pl_vm_size_t cmd_offset = image->header_addr + image->header_size;
    image->ncmds = image->swap32(image->header.ncmds);
    plcrash_error_t ret = plcrash_async_mobject_init(&image->load_cmds, image->task, cmd_offset, cmd_len, true);
    if (ret != PLCRASH_ESUCCESS) {
        PLCF_DEBUG("Failed to map Mach-O load commands in image %s", image->name);
        return ret;
    }

    /* Now that the image has been sufficiently initialized, determine the __TEXT segment size */
    void *cmdptr = NULL;
    image->text_size = 0x0;
    bool found_text_seg = false;
    while ((cmdptr = plcrash_async_macho_next_command_type(image, cmdptr, image->m64 ? LC_SEGMENT_64 : LC_SEGMENT)) != 0) {
        if (image->m64) {
            struct segment_command_64 *segment = cmdptr;
            if (!plcrash_async_mobject_verify_local_pointer(&image->load_cmds, (uintptr_t)segment, sizeof(*segment))) {
                PLCF_DEBUG("LC_SEGMENT command was too short");
                return PLCRASH_EINVAL;
            }
            
            if (plcrash_async_strncmp(segment->segname, SEG_TEXT, sizeof(segment->segname)) != 0)
                continue;
            
            image->text_size = image->swap64(segment->vmsize);
            found_text_seg = true;
            break;
        } else {
            struct segment_command *segment = cmdptr;
            if (!plcrash_async_mobject_verify_local_pointer(&image->load_cmds, (uintptr_t)segment, sizeof(*segment))) {
                PLCF_DEBUG("LC_SEGMENT command was too short");
                return PLCRASH_EINVAL;
            }
            
            if (plcrash_async_strncmp(segment->segname, SEG_TEXT, sizeof(segment->segname)) != 0)
                continue;
            
            image->text_size = image->swap32(segment->vmsize);
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
 * @return Returns the address of the next load_command on success, or NULL on failure.
 *
 * @note A returned command is gauranteed to be readable, and fully within mapped address space. If the command
 * command can not be verified to have available MAX(sizeof(struct load_command), cmd->cmdsize) bytes, NULL will be
 * returned.
 */
void *plcrash_async_macho_next_command (plcrash_async_macho_t *image, void *previous) {
    struct load_command *cmd;

    /* On the first iteration, determine the LC_CMD offset from the Mach-O header. */
    if (previous == NULL) {
        /* Sanity check */
        if (image->swap32(image->header.sizeofcmds) < sizeof(struct load_command)) {
            PLCF_DEBUG("Mach-O sizeofcmds is less than sizeof(struct load_command) in %s", image->name);
            return NULL;
        }

        return plcrash_async_mobject_remap_address(&image->load_cmds, image->header_addr + image->header_size,
                                                   sizeof(struct load_command));
    }

    /* We need the size from the previous load command; first, verify the pointer. */
    cmd = previous;
    if (!plcrash_async_mobject_verify_local_pointer(&image->load_cmds, (uintptr_t) cmd, sizeof(*cmd))) {
        PLCF_DEBUG("Failed to map LC_CMD at address %p in: %s", cmd, image->name);
        return NULL;
    }

    /* Advance to the next command */
    uint32_t cmdsize = image->swap32(cmd->cmdsize);
    void *next = ((uint8_t *)previous) + cmdsize;

    /* Avoid walking off the end of the cmd buffer */
    if ((uintptr_t)next >= image->load_cmds.address + image->load_cmds.length)
        return NULL;

    /* Verify that it holds at least load_command */
    if (!plcrash_async_mobject_verify_local_pointer(&image->load_cmds, (uintptr_t) next, sizeof(struct load_command))) {
        PLCF_DEBUG("Failed to map LC_CMD at address %p in: %s", cmd, image->name);
        return NULL;
    }

    /* Verify the actual size. */
    cmd = next;
    if (!plcrash_async_mobject_verify_local_pointer(&image->load_cmds, (uintptr_t) next, image->swap32(cmd->cmdsize))) {
        PLCF_DEBUG("Failed to map LC_CMD at address %p in: %s", cmd, image->name);
        return NULL;
    }

    return next;
}

/**
 * Iterate over the available Mach-O LC_CMD entries.
 *
 * @param image The image to iterate
 * @param previous The previously returned LC_CMD address value, or 0 to iterate from the first LC_CMD.
 * @param expectedCommand The LC_* command type to be returned. Only commands matching this type will be returned by the iterator.
 * @return Returns the address of the next load_command on success, or 0 on failure. 
 *
 * @note A returned command is gauranteed to be readable, and fully within mapped address space. If the command
 * command can not be verified to have available MAX(sizeof(struct load_command), cmd->cmdsize) bytes, NULL will be
 * returned.
 */
void *plcrash_async_macho_next_command_type (plcrash_async_macho_t *image, void *previous, uint32_t expectedCommand) {
    struct load_command *cmd = previous;

    /* Iterate commands until we either find a match, or reach the end */
    while ((cmd = plcrash_async_macho_next_command(image, cmd)) != NULL) {
        /* Return a match */
        if (image->swap32(cmd->cmd) == expectedCommand) {
            return cmd;
        }
    }

    /* No match found */
    return NULL;
}

/**
 * Find the first LC_CMD matching the given @a cmd type.
 *
 * @param image The image to search.
 * @param expectedCommand The LC_CMD type to find.
 *
 * @return Returns the address of the matching load_command on success, or 0 on failure.
 *
 * @note A returned command is gauranteed to be readable, and fully within mapped address space. If the command
 * command can not be verified to have available MAX(sizeof(struct load_command), cmd->cmdsize) bytes, NULL will be
 * returned.
 */
void *plcrash_async_macho_find_command (plcrash_async_macho_t *image, uint32_t expectedCommand) {
    struct load_command *cmd = NULL;

    /* Iterate commands until we either find a match, or reach the end */
    while ((cmd = plcrash_async_macho_next_command(image, cmd)) != NULL) {
        /* Read the load command type */
        if (!plcrash_async_mobject_verify_local_pointer(&image->load_cmds, (uintptr_t) cmd, sizeof(*cmd))) {
            PLCF_DEBUG("Failed to map LC_CMD at address %p in: %s", cmd, image->name);
            return NULL;
        }

        /* Return a match */
        if (image->swap32(cmd->cmd) == expectedCommand) {
            return cmd;
        }
    }
    
    /* No match found */
    return NULL;
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
 * @return Returns a mapped pointer to the segment on success, or NULL on failure.
 */
void *plcrash_async_macho_find_segment_cmd (plcrash_async_macho_t *image, const char *segname) {
    void *seg = NULL;

    while ((seg = plcrash_async_macho_next_command_type(image, seg, image->m64 ? LC_SEGMENT_64 : LC_SEGMENT)) != 0) {

        /* Read the load command */
        if (image->m64) {
            struct segment_command_64 *cmd_64 = seg;
            if (plcrash_async_strncmp(segname, cmd_64->segname, sizeof(cmd_64->segname)) == 0)
                return seg;
        } else {
            struct segment_command *cmd_32 = seg;
            if (plcrash_async_strncmp(segname, cmd_32->segname, sizeof(cmd_32->segname)) == 0)
                return seg;
        }
    }

    return NULL;
}

/**
 * Find and map a named segment, initializing @a mobj. It is the caller's responsibility to dealloc @a mobj after
 * a successful initialization
 *
 * @param image The image to search for @a segname.
 * @param segname The name of the segment to be mapped.
 * @param seg The segment data to be initialized. It is the caller's responsibility to dealloc @a seg after
 * a successful initialization.
 *
 * @warning Due to bugs in the update_dyld_shared_cache(1), the segment vmsize defined in the Mach-O load commands may
 * be invalid, and the declared size may be unmappable. As such, it is possible that this function will return a mapping
 * that is less than the total requested size. All accesses to this mapping should be done (as is already the norm)
 * through range-checked pointer validation. This bug appears to be caused by a bug in computing the correct vmsize
 * when update_dyld_shared_cache(1) generates the single shared LINKEDIT segment, and has been reported to Apple
 * as rdar://13707406.
 *
 * @return Returns PLCRASH_ESUCCESS on success, or an error result on failure.
 */
plcrash_error_t plcrash_async_macho_map_segment (plcrash_async_macho_t *image, const char *segname, pl_async_macho_mapped_segment_t *seg) {
    struct segment_command *cmd_32;
    struct segment_command_64 *cmd_64;
    
    void *segment =  plcrash_async_macho_find_segment_cmd(image, segname);
    if (segment == NULL)
        return PLCRASH_ENOTFOUND;

    cmd_32 = segment;
    cmd_64 = segment;

    /* Calculate the in-memory address and size */
    pl_vm_address_t segaddr;
    pl_vm_size_t segsize;
    if (image->m64) {
        segaddr = image->swap64(cmd_64->vmaddr) + image->vmaddr_slide;
        segsize = image->swap64(cmd_64->vmsize);

        seg->fileoff = image->swap64(cmd_64->fileoff);
        seg->filesize = image->swap64(cmd_64->filesize);
    } else {
        segaddr = image->swap32(cmd_32->vmaddr) + image->vmaddr_slide;
        segsize = image->swap32(cmd_32->vmsize);
        
        seg->fileoff = image->swap32(cmd_32->fileoff);
        seg->filesize = image->swap32(cmd_32->filesize);
    }

    /* Perform and return the mapping (permitting shorter mappings, as documented above). */
    return plcrash_async_mobject_init(&seg->mobj, image->task, segaddr, segsize, false);
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
plcrash_error_t plcrash_async_macho_map_section (plcrash_async_macho_t *image, const char *segname, const char *sectname, plcrash_async_mobject_t *mobj) {
    struct segment_command *cmd_32;
    struct segment_command_64 *cmd_64;
    
    void *segment =  plcrash_async_macho_find_segment_cmd(image, segname);
    if (segment == NULL)
        return PLCRASH_ENOTFOUND;

    cmd_32 = segment;
    cmd_64 = segment;
    
    uint32_t nsects;
    uintptr_t cursor = (uintptr_t) segment;

    if (image->m64) {
        nsects = cmd_64->nsects;
        cursor += sizeof(*cmd_64);
    } else {
        nsects = cmd_32->nsects;
        cursor += sizeof(*cmd_32);
    }

    for (uint32_t i = 0; i < nsects; i++) {        
        struct section *sect_32 = NULL;
        struct section_64 *sect_64 = NULL;
       
        if (image->m64) {
            if (!plcrash_async_mobject_verify_local_pointer(&image->load_cmds, cursor, sizeof(*sect_64))) {
                PLCF_DEBUG("Section table entry outside of expected range; searching for (%s,%s)", segname, sectname);
                return PLCRASH_EINVAL;
            }
            
            sect_64 = (void *) cursor;
            cursor += sizeof(*sect_64);
        } else {
            if (!plcrash_async_mobject_verify_local_pointer(&image->load_cmds, cursor, sizeof(*sect_32))) {
                PLCF_DEBUG("Section table entry outside of expected range; searching for (%s,%s)", segname, sectname);
                return PLCRASH_EINVAL;
            }
            
            sect_32 = (void *) cursor;
            cursor += sizeof(*sect_32);
        }
        
        const char *image_sectname = image->m64 ? sect_64->sectname : sect_32->sectname;
        if (plcrash_async_strncmp(sectname, image_sectname, sizeof(sect_64->sectname)) == 0) {
            /* Calculate the in-memory address and size */
            pl_vm_address_t sectaddr;
            pl_vm_size_t sectsize;
            if (image->m64) {
                sectaddr = image->swap64(sect_64->addr) + image->vmaddr_slide;
                sectsize = image->swap32(sect_64->size);
            } else {
                sectaddr = image->swap32(sect_32->addr) + image->vmaddr_slide;
                sectsize = image->swap32(sect_32->size);
            }
            
            
            /* Perform and return the mapping */
            return plcrash_async_mobject_init(mobj, image->task, sectaddr, sectsize, true);
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
static bool plcrash_async_macho_find_symtab_symbol (plcrash_async_macho_t *image, pl_vm_address_t slide_pc, pl_nlist_common *symtab,
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
     * plcrash_async_mobject_remap_address() above. */
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
plcrash_error_t plcrash_async_macho_find_symbol (plcrash_async_macho_t *image, pl_vm_address_t pc, pl_async_macho_found_symbol_cb symbol_cb, void *context) {
    plcrash_error_t retval;
    
    /* Compute the actual in-core PC. */
    pl_vm_address_t slide_pc = pc - image->vmaddr_slide;

    /* Fetch the symtab commands, if available. */
    struct symtab_command *symtab_cmd = plcrash_async_macho_find_command(image, LC_SYMTAB);
    struct dysymtab_command *dysymtab_cmd = plcrash_async_macho_find_command(image, LC_DYSYMTAB);


    /* The symtab command is required */
    if (symtab_cmd == NULL) {
        PLCF_DEBUG("could not find LC_SYMTAB load command");
        return PLCRASH_ENOTFOUND;
    }

    /* Map in the __LINKEDIT segment, which includes the symbol and string tables. */
    pl_async_macho_mapped_segment_t linkedit_seg;
    plcrash_error_t err = plcrash_async_macho_map_segment(image, "__LINKEDIT", &linkedit_seg);
    if (err != PLCRASH_ESUCCESS) {
        PLCF_DEBUG("plcrash_async_mobject_init() failure: %d in %s", err, image->name);
        return PLCRASH_EINTERNAL;
    }

    /* Determine the string and symbol table sizes. */
    uint32_t nsyms = image->swap32(symtab_cmd->nsyms);
    size_t nlist_struct_size = image->m64 ? sizeof(struct nlist_64) : sizeof(struct nlist);
    size_t nlist_table_size = nsyms * nlist_struct_size;

    size_t string_size = image->swap32(symtab_cmd->strsize);

    /* Fetch pointers to the symbol and string tables, and verify their size values */
    void *nlist_table;
    char *string_table;

    nlist_table = plcrash_async_mobject_remap_address(&linkedit_seg.mobj,
                                                      linkedit_seg.mobj.task_address + (image->swap32(symtab_cmd->symoff) - linkedit_seg.fileoff),
                                                      nlist_table_size);
    if (nlist_table == NULL) {
        PLCF_DEBUG("plcrash_async_mobject_remap_address(mobj, %" PRIx64 ", %" PRIx64") returned NULL mapping __LINKEDIT.symoff in %s",
                   (uint64_t) linkedit_seg.mobj.address + image->swap32(symtab_cmd->symoff), (uint64_t) nlist_table_size, image->name);
        retval = PLCRASH_EINTERNAL;
        goto cleanup;
    }

    string_table = plcrash_async_mobject_remap_address(&linkedit_seg.mobj,
                                                       linkedit_seg.mobj.task_address + (image->swap32(symtab_cmd->stroff) - linkedit_seg.fileoff),
                                                       string_size);
    if (string_table == NULL) {
        PLCF_DEBUG("plcrash_async_mobject_remap_address(mobj, %" PRIx64 ", %" PRIx64") returned NULL mapping __LINKEDIT.stroff in %s",
                   (uint64_t) linkedit_seg.mobj.address + image->swap32(symtab_cmd->stroff), (uint64_t) string_size, image->name);
        retval = PLCRASH_EINTERNAL;
        goto cleanup;
    }
            
    /* Walk the symbol table. We know that the full range of symbols[nsyms-1] is valid, since we fetched a pointer+len
     * based on the value using plcrash_async_mobject_remap_address() above. */
    pl_nlist_common *found_symbol = NULL;

    if (dysymtab_cmd != NULL) {
        /* dysymtab is available; use it to constrain our symbol search to the global and local sections of the symbol table. */

        uint32_t idx_syms_global = image->swap32(dysymtab_cmd->iextdefsym);
        uint32_t idx_syms_local = image->swap32(dysymtab_cmd->ilocalsym);
        
        uint32_t nsyms_global = image->swap32(dysymtab_cmd->nextdefsym);
        uint32_t nsyms_local = image->swap32(dysymtab_cmd->nlocalsym);

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

        plcrash_async_macho_find_symtab_symbol(image, slide_pc, global_nlist, nsyms_global, &found_symbol);
        plcrash_async_macho_find_symtab_symbol(image, slide_pc, local_nlist, nsyms_local, &found_symbol);
    } else {
        /* If dysymtab is not available, search all symbols */
        plcrash_async_macho_find_symtab_symbol(image, slide_pc, nlist_table, nsyms, &found_symbol);
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
     * TODO: Evaluate effeciency of per-byte calling of plcrash_async_mobject_verify_local_pointer(). We should
     * probably validate whole pages at a time instead.
     */
    const char *sym_name = string_table + image->swap32(found_symbol->n32.n_un.n_strx);
    const char *p = sym_name;
    do {
        if (!plcrash_async_mobject_verify_local_pointer(&linkedit_seg.mobj, (uintptr_t) p, 1)) {
            PLCF_DEBUG("End of mobject reached while walking string\n");
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
    plcrash_async_macho_mapped_segment_free(&linkedit_seg);
    return retval;
}

/**
 * Free all mapped segment resources.
 *
 * @note Unlike most free() functions in this API, this function is async-safe.
 */
void plcrash_async_macho_mapped_segment_free (pl_async_macho_mapped_segment_t *segment) {
    plcrash_async_mobject_free(&segment->mobj);
}

/**
 * Free all Mach-O binary image resources.
 *
 * @warning This method is not async safe.
 */
void plcrash_nasync_macho_free (plcrash_async_macho_t *image) {
    if (image->name != NULL)
        free(image->name);
    
    plcrash_async_mobject_free(&image->load_cmds);

    mach_port_mod_refs(mach_task_self(), image->task, MACH_PORT_RIGHT_SEND, -1);
}


/**
 * @} pl_async_macho
 */