/*
 * Author: Landon Fuller <landonf@plausible.coop>
 *
 * Copyright (c) 2015 Plausible Labs Cooperative, Inc.
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

#include "PLCrashMacros.h"
#include "PLCrashDLCompat.h"

#include "AsyncAllocator.hpp"
#include "DynamicLoader.hpp"
#include "PLCrashAsyncMachOImage.h"
#include <errno.h>

#include <dlfcn.h>
#include <mach-o/dyld.h>

using namespace plcrash::async;

/**
 * @internal
 * @ingroup plcrash_test_dlfcn
 * @{
 */

/* Copied from the Mach-O API implementation. If dladdr() isn't fixed and this code sticks around, we'll
 * need to unify these two implementations */
static void plcrash_async_macho_find_best_symbol (plcrash_async_macho_symtab_reader_t *reader,
                                                  pl_vm_address_t slide_pc,
                                                  void *symtab, uint32_t nsyms,
                                                  plcrash_async_macho_symtab_entry_t *found_symbol,
                                                  plcrash_async_macho_symtab_entry_t *prev_symbol,
                                                  bool *did_find_symbol)
{
    plcrash_async_macho_symtab_entry_t new_entry;
    
    /* Set did_find_symbol to false by default */
    if (prev_symbol == NULL)
        *did_find_symbol = false;
    
    /* Walk the symbol table. We know that symbols[i] is valid, since we fetched a pointer+len based on the value using
     * plcrash_async_mobject_remap_address() above. */
    for (uint32_t i = 0; i < nsyms; i++) {
        new_entry = plcrash_async_macho_symtab_reader_read(reader, symtab, i);
        
        /* Symbol must be within a section, and must not be a debugging entry. */
        if ((new_entry.n_type & N_TYPE) != N_SECT || ((new_entry.n_type & N_STAB) != 0))
            continue;
        
        /* Search for the best match. We're looking for the closest symbol occuring before PC. */
        if (new_entry.n_value <= slide_pc && (!*did_find_symbol || prev_symbol->n_value < new_entry.n_value)) {
            *found_symbol = new_entry;
            
            /* The newly found symbol is now the symbol to be matched against */
            prev_symbol = found_symbol;
            *did_find_symbol = true;
        }
    }
}

/**
 * API-compatible replacement for dladdr().
 */
PLCR_EXPORT int pl_dladdr (const void *addr, Dl_info *info) {
    AsyncAllocator *allocator;
    plcrash_error_t err;
    
    
    /* Create our allocator.
     * TODO: Once we have a polymorphic allocator API, we should use the system allocator here instead
     * of an async-safe allocator. */
    if ((err = AsyncAllocator::Create(&allocator, 64 * 1024 * 1024 /* 64KB */)) != PLCRASH_ESUCCESS) {
        return 0;
    }
    
    /* Fetch our image list */
    DynamicLoader::ImageList *imageList;
    if ((err = DynamicLoader::ImageList::NonAsync_Read(&imageList, allocator, mach_task_self())) != PLCRASH_ESUCCESS) {
        delete allocator;
        return 0;
    }
    
    /* Find our image */
    plcrash_async_macho_t *image = imageList->imageContainingAddress((pl_vm_address_t) addr);
    if (image == nullptr) {
        delete imageList;
        delete allocator;

        return 0;
    }
    
    /*
     * Save the image base address and name.
     *
     * We need a persistent pointer to the image's file name that will survive at least as long as the image does.
     * We use dyld (this API still works) for now -- in the future, we might want to preserve the original name
     * address in the plcrash_async_macho_t instance.
     */
    info->dli_fbase = (void *) image->header_addr;
    info->dli_fname = nullptr;
    
    for (size_t i = 0; i < _dyld_image_count(); i++) {
        if ((pl_vm_address_t) _dyld_get_image_header(i) != image->header_addr)
            continue;
        
        info->dli_fname = _dyld_get_image_name(i);
        break;
    }
    
    if (info->dli_fname == nullptr) {
        PLCF_DEBUG("pl_dladdr() failed unexpectedly: could not fetch the image path!");
        
        delete imageList;
        delete allocator;
        return 0;
    }
    
    
    
    /* Try to find the nearest symbol */
    plcrash_async_macho_symtab_reader_t symtab;
    if ((err = plcrash_async_macho_symtab_reader_init(&symtab, image)) != PLCRASH_ESUCCESS) {
        
        delete imageList;
        delete allocator;
        return 0;
    }
    

    /* Compute the on-disk address. */
    pl_vm_address_t slide_addr = (pl_vm_address_t) addr - image->vmaddr_slide;
    

    /* Walk the symbol table. */
    plcrash_async_macho_symtab_entry_t found_symbol;
    bool did_find_symbol;
    
    if (symtab.symtab_global != NULL && symtab.symtab_local != NULL) {
        /* dysymtab is available; use it to constrain our symbol search to the global and local sections of the symbol table. */
        plcrash_async_macho_find_best_symbol(&symtab, slide_addr, symtab.symtab_global, symtab.nsyms_global, &found_symbol, NULL, &did_find_symbol);
        plcrash_async_macho_find_best_symbol(&symtab, slide_addr, symtab.symtab_local, symtab.nsyms_local, &found_symbol, &found_symbol, &did_find_symbol);
    } else {
        /* If dysymtab is not available, search all symbols */
        plcrash_async_macho_find_best_symbol(&symtab, slide_addr, symtab.symtab, symtab.nsyms, &found_symbol, NULL, &did_find_symbol);
    }
    
    if (did_find_symbol) {
        /* Find the non-mobject-mapped address of the string table */
        char *string_table = (char *) ((pl_vm_address_t) symtab.string_table + symtab.linkedit.mobj.vm_slide);
        info->dli_sname = string_table + found_symbol.n_strx;
        info->dli_saddr = (void *) (found_symbol.normalized_value + image->vmaddr_slide);
    } else {
        info->dli_sname = nullptr;
        info->dli_saddr = 0x0;
    }

    /* Done! */
    plcrash_async_macho_symtab_reader_free(&symtab);
    delete imageList;
    delete allocator;

    return 1;
}

/**
 * @}
 */
