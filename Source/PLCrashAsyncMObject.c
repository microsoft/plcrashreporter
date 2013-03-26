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

#ifdef PL_HAVE_BROKEN_VM_REMAP

/**
 * Verify the validity of the given @a address and @a length within the current process. Note that this validity
 * is only gauranteed insofar as the pages in question are not unmapped, which may occur for any reason, including
 * the case where the process' threads have not been suspended.
 *
 * @param address The target address to verify.
 * @param length The total size of the range to be verified.
 *
 * @warning This function is provided as a work-around for bugs in vm_remap() that have been reported
 * in iOS 6. Should those bugs be isolated and fixed, this implementation may be removed.
 */
static plcrash_error_t plcrash_async_mobject_vm_regions_valid (pl_vm_address_t address, pl_vm_size_t length) {
    kern_return_t kt;
    
    if (length == 0)
        return PLCRASH_ESUCCESS;

    pl_vm_address_t start_address = address;

    while (start_address < address+length) {
        mach_msg_type_number_t info_count;
        pl_vm_address_t region_base;
        pl_vm_size_t region_size;
        natural_t nesting_depth = 0;

        region_base = start_address;

        PLCF_DEBUG("Recursing vm region for address=0x%" PRIx64 " looking for terminator=0x%" PRIx64, (uint64_t) region_base, (uint64_t) (address+length));
        
#ifdef PL_HAVE_MACH_VM
        vm_region_submap_info_data_64_t info;
        info_count = VM_REGION_SUBMAP_INFO_COUNT_64;

        kt = mach_vm_region_recurse(mach_task_self(), &region_base, &region_size, &nesting_depth, (vm_region_recurse_info_t) &info, &info_count);
#else
        vm_region_submap_info_data_t info;
        info_count = VM_REGION_SUBMAP_INFO_COUNT;

        kt = vm_region_recurse(mach_task_self(), &region_base, &region_size, &nesting_depth, (vm_region_recurse_info_t) &info, &info_count);
#endif
        
        if (kt != KERN_SUCCESS) {
            PLCF_DEBUG("Failed to recurse vm region for address=0x%" PRIx64" length=0x%" PRIx64 ": %x",
                (uint64_t) region_base, (uint64_t) (region_size), kt);
            return PLCRASH_EINTERNAL;
        }
        
        if ((info.protection & VM_PROT_READ) == 0) {
            PLCF_DEBUG("Missing read permissions for address=0x%" PRIx64" length=0x%" PRIx64 ": %x",
                       (uint64_t) region_base, (uint64_t) (region_size), kt);
            return PLCRASH_EACCESS;
        }
        
        PLCF_DEBUG("Found vm region for address=0x%" PRIx64" length=0x%" PRIx64,
                   (uint64_t) region_base, (uint64_t) (region_size));

        start_address = region_base + region_size;
    }

    return PLCRASH_ESUCCESS;
}

#endif

/**
 * Initialize a new memory object reference, mapping @a task_addr from @a task into the current process. The mapping
 * will be copy-on-write, and will be checked to ensure a minimum protection value of VM_PROT_READ.
 *
 * @param mobj Memory object to be initialized.
 * @param task The task from which the memory will be mapped.
 * @param task_address The task-relative address of the memory to be mapped. This is not required to fall on a page boundry.
 * @param length The total size of the mapping to create.
 *
 * @return On success, returns PLCRASH_ESUCCESS. On failure, one of the plcrash_error_t error values will be returned, and no
 * mapping will be performed.
 *
 * @warn Callers must call plcrash_async_mobject_free() on @a mobj, even if plcrash_async_mobject_init() fails.
 */
plcrash_error_t plcrash_async_mobject_init (plcrash_async_mobject_t *mobj, mach_port_t task, pl_vm_address_t task_addr, pl_vm_size_t length) {
#ifdef PL_HAVE_BROKEN_VM_REMAP
    if (plcrash_async_mobject_vm_regions_valid(task_addr, length) != PLCRASH_ESUCCESS)
        return PLCRASH_ENOMEM;

    /* This operation mode is unsupported when running out-of-process */
    PLCF_ASSERT(task == mach_task_self());

    mobj->vm_address = 0x0;
    mobj->address = task_addr;
    mobj->length = length;
    mobj->vm_slide = 0;
    mobj->task_address = task_addr;
    
    return PLCRASH_ESUCCESS;
    
#else
    /* We must first initialize vm_address to 0x0. We'll check this in _free() to determine whether calling vm_deallocate() is required */
    mobj->vm_address = 0x0;
    
    kern_return_t kt;
    
    /* Compute the total required page size. */
    pl_vm_size_t page_size = mach_vm_round_page(length + (task_addr - mach_vm_trunc_page(task_addr)));
    
    /* Remap the target pages into our process */
    vm_prot_t cur_prot;
    vm_prot_t max_prot;
    
#ifdef PL_HAVE_MACH_VM
    kt = mach_vm_remap(mach_task_self(), &mobj->vm_address, page_size, 0x0, TRUE, task, task_addr, FALSE, &cur_prot, &max_prot, VM_INHERIT_COPY);
#else
    kt = vm_remap(mach_task_self(), &mobj->vm_address, page_size, 0x0, TRUE, task, task_addr, FALSE, &cur_prot, &max_prot, VM_INHERIT_COPY);
#endif
    
    if (kt != KERN_SUCCESS) {
        PLCF_DEBUG("vm_remap() failure: %d", kt);
        // Should we use more descriptive errors?
        return PLCRASH_ENOMEM;
    }
    
    if ((cur_prot & VM_PROT_READ) == 0) {
        return PLCRASH_EACCESS;
    }
    
    /* Determine the offset to the actual data */
    mobj->address = mobj->vm_address + (task_addr - mach_vm_trunc_page(task_addr));
    mobj->length = length;
    
    /* Determine the difference between the target and local mappings. Note that this needs to be computed on either two page
     * aligned addresses, or two non-page aligned addresses. Mixing task_addr and vm_address would return an incorrect offset. */
    mobj->vm_slide = task_addr - mobj->address;
    
    /* Save the task-relative address */
    mobj->task_address = task_addr;

    return PLCRASH_ESUCCESS;
#endif /* !PL_HAVE_BROKEN_VM_REMAP */
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