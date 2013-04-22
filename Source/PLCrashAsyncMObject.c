/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2012-2013 Plausible Labs Cooperative, Inc.
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

#import "PLCrashAsyncMObject.h"

#import <stdint.h>
#import <inttypes.h>

/**
 * @internal
 * @ingroup plcrash_async
 *
 * Implements async-safe cross-task memory mapping.
 *
 * @{
 */


/**
 * Initialize a new memory object reference, mapping @a task_addr from @a task into the current process. The mapping
 * will be copy-on-write, and will be checked to ensure a minimum protection value of VM_PROT_READ.
 *
 * @param mobj Memory object to be initialized.
 * @param task The task from which the memory will be mapped.
 * @param task_address The task-relative address of the memory to be mapped. This is not required to fall on a page boundry.
 * @param length The total size of the mapping to create.
 * @param require_full If false, short mappings will be permitted in the case where a memory object of the requested length
 * does not exist at the target address. It is the caller's responsibility to validate the resulting length of the
 * mapping, eg, using plcrash_async_mobject_remap_address() and similar. If true, and the entire requested page range is
 * not valid, the mapping request will fail.
 *
 * @return On success, returns PLCRASH_ESUCCESS. On failure, one of the plcrash_error_t error values will be returned, and no
 * mapping will be performed.
 *
 * @warn Callers must call plcrash_async_mobject_free() on @a mobj, even if plcrash_async_mobject_init() fails.
 */
plcrash_error_t plcrash_async_mobject_init (plcrash_async_mobject_t *mobj, mach_port_t task, pl_vm_address_t task_addr, pl_vm_size_t length, bool require_full) {
    /* We must first initialize vm_address to 0x0. We'll check this in _free() to determine whether calling vm_deallocate() is required */
    mobj->vm_address = 0x0;
    
    kern_return_t kt;
    
    /* Compute the total required page size. */
    pl_vm_address_t page_addr = mach_vm_trunc_page(task_addr);
    pl_vm_size_t page_size = mach_vm_round_page(length + (task_addr - page_addr));

    /* Remap the target pages into our process */
#ifdef PL_HAVE_BROKEN_VM_REMAP
    /* Memory object implementation */
    memory_object_size_t entry_length = page_size;
    mach_port_t mem_handle;
    kt = mach_make_memory_entry_64(task, &entry_length, task_addr, VM_PROT_READ, &mem_handle, MACH_PORT_NULL);
    if (kt != KERN_SUCCESS) {
        PLCF_DEBUG("mach_make_memory_entry_64() failed: %d", kt);
        return PLCRASH_ENOMEM;
    }

    /*
     * If short mappings are enabled, and the entry is smaller than the target mapping, use the memory entry's
     * size rather than the originally requested size. Otherwise, a smaller entry will result in a vm_map() 
     * error when the requested pages are unavailable.
     */
    if (!require_full && entry_length < page_size) {
        /* Reset the page size to match the actual available memory ... */
        page_size = entry_length;

        /* The length represents the user's requested byte length, and is not required to be page aligned. Thus, it
         * needs to be recomputed such that it fits within the smaller entry size here, while still accounting for the
         * offset of the user's non-page-aligned start address. */
        length = entry_length - (task_addr - page_addr);
    }

#ifdef PL_HAVE_MACH_VM
    kt = mach_vm_map(mach_task_self(), &mobj->vm_address, page_size, 0x0, VM_FLAGS_ANYWHERE, mem_handle, 0x0, TRUE, VM_PROT_READ, VM_PROT_READ, VM_INHERIT_COPY);
#else
    kt = vm_map(mach_task_self(), &mobj->vm_address, page_size, 0x0, VM_FLAGS_ANYWHERE, mem_handle, 0x0, TRUE, VM_PROT_READ, VM_PROT_READ, VM_INHERIT_COPY);
#endif /* !PL_HAVE_MACH_VM */

    if (kt != KERN_SUCCESS) {
        PLCF_DEBUG("vm_map() failure: %d", kt);
        
        kt = mach_port_mod_refs(mach_task_self(), mem_handle, MACH_PORT_RIGHT_SEND, -1);
        if (kt != KERN_SUCCESS) {
            PLCF_DEBUG("mach_port_mod_refs(-1) failed: %d", kt);
        }
        
        return PLCRASH_ENOMEM;
    }
    
    kt = mach_port_mod_refs(mach_task_self(), mem_handle, MACH_PORT_RIGHT_SEND, -1);
    if (kt != KERN_SUCCESS) {
        PLCF_DEBUG("mach_port_mod_refs(-1) failed: %d", kt);
    }
    
#else
    /* vm_remap() implementation */
    vm_prot_t cur_prot;
    vm_prot_t max_prot;

#ifdef PL_HAVE_MACH_VM
    kt = mach_vm_remap(mach_task_self(), &mobj->vm_address, page_size, 0x0, TRUE, task, task_addr, FALSE, &cur_prot, &max_prot, VM_INHERIT_COPY);
#else
    kt = vm_remap(mach_task_self(), &mobj->vm_address, page_size, 0x0, TRUE, task, task_addr, FALSE, &cur_prot, &max_prot, VM_INHERIT_COPY);
#endif /* !PL_HAVE_MACH_VM */
    
    if (kt != KERN_SUCCESS) {
        PLCF_DEBUG("vm_remap() failure: %d", kt);
        // Should we use more descriptive errors?
        return PLCRASH_ENOMEM;
    }
    
    if ((cur_prot & VM_PROT_READ) == 0) {
        return PLCRASH_EACCESS;
    }
    
#endif /* PL_HAVE_BROKEN_VM_REMAP */
    
    /* Determine the offset to the actual data */
    mobj->address = mobj->vm_address + (task_addr - mach_vm_trunc_page(task_addr));
    mobj->length = length;
    
    /* Determine the difference between the target and local mappings. Note that this needs to be computed on either two page
     * aligned addresses, or two non-page aligned addresses. Mixing task_addr and vm_address would return an incorrect offset. */
    mobj->vm_slide = task_addr - mobj->address;
    
    /* Save the task-relative address */
    mobj->task_address = task_addr;

    return PLCRASH_ESUCCESS;
}


/**
 * Verify that @a length bytes starting at local @a address is within @a mobj's mapped range.
 *
 * @param mobj An initialized memory object.
 * @param address An address within the current task's memory space.
 * @param length The number of bytes that should be readable at @a address.
 */
bool plcrash_async_mobject_verify_local_pointer (plcrash_async_mobject_t *mobj, uintptr_t address, size_t length) {
    /* Verify that the address starts within range */
    if (address < mobj->address) {
        // PLCF_DEBUG("Address %" PRIx64 " < base address %" PRIx64 "", (uint64_t) address, (uint64_t) mobj->address);
        return false;
    }
    
    /* Verify that the address value won't overrun */
    if (UINTPTR_MAX - length < address)
        return false;
    
    /* Check that the block ends within range */
    if (mobj->address + mobj->length < address + length) {
        // PLCF_DEBUG("Address %" PRIx64 " out of range %" PRIx64 " + %" PRIx64, (uint64_t) address, (uint64_t) mobj->address, (uint64_t) mobj->length);
        return false;
    }

    return true;
}

/**
 * Validate a target process' address pointer's availability via @a mobj, verifying that @a length bytes can be read
 * from @a mobj at @a address, and return the pointer from which a @a length read may be performed.
 *
 * @param mobj An initialized memory object.
 * @param address The address to be read. This address should be relative to the target task's address space.
 * @param length The total number of bytes that should be readable at @a address.
 *
 * @return Returns the validated pointer, or NULL if the requested bytes are not within @a mobj's range.
 */
void *plcrash_async_mobject_remap_address (plcrash_async_mobject_t *mobj, pl_vm_address_t address, size_t length) {
    /* Map into our memory space */
    pl_vm_address_t remapped = address - mobj->vm_slide;
    
    if (!plcrash_async_mobject_verify_local_pointer(mobj, (uintptr_t) remapped, length))
        return NULL;

    return (void *) remapped;
}

/**
 * Free the memory mapping.
 *
 * @note Unlike most free() functions in this API, this function is async-safe.
 */
void plcrash_async_mobject_free (plcrash_async_mobject_t *mobj) {
    if (mobj->vm_address == 0x0)
        return;
    
    kern_return_t kt;
    if ((kt = vm_deallocate(mach_task_self(), mobj->address, mobj->length)) != KERN_SUCCESS)
        PLCF_DEBUG("vm_deallocate() failure: %d", kt);
}

/**
 * @} plcrash_async
 */