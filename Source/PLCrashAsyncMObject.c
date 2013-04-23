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
 * @note
 * This function previously used vm_remap() to perform atomic remapping of process memory. However, this appeared
 * to trigger a kernel bug (and resulting panic) on iOS 6.0 through 6.1.2, possibly fixed in 6.1.3. Note that
 * no stable release of PLCrashReporter shipped with the vm_remap() code.
 *
 * Investigation of the failure seems to show an over-release of the target vm_map and backing vm_object, leading to
 * NULL dereference, invalid memory references, and in some cases, deadlocks that result in watchdog timeouts.
 *
 * In one example case, the crash occurs in update_first_free_ll() as a NULL dereference of the vm_map_entry_t parameter.
 * Analysis of the limited reports shows that this is called via vm_map_store_update_first_free(). No backtrace is
 * available from the kernel panics, but analyzing the register state demonstrates:
 * - A reference to vm_map_store_update_first_free() remains in the link register.
 * - Of the following callers, one can be eliminated by register state:
 *     - vm_map_enter - not possible, r3 should be equal to r0
 *     - vm_map_clip_start - possible
 *     - vm_map_clip_unnest - possible
 *     - vm_map_clip_end - possible
 *
 * In the other panic seen in vm_object_reap_pages(), a value of 0x8008 is loaded and deferenced from the next pointer
 * of an element within the vm_object's resident page queue (object->memq).
 *
 * Unfortunately, our ability to investigate has been extremely constrained by the following issues;
 * - The panic is not easily or reliably reproducible
 * - Apple's does not support iOS kernel debugging
 * - There is no support for jailbreak kernel debugging against iOS 6.x devices at the time of writing.
 *
 * The work-around used here is to split the vm_remap() into distinct calls to mach_make_memory_entry_64() and
 * vm_map(); this follows a largely distinct code path from vm_remap(). In testing by a large-scale user of PLCrashReporter,
 * they were no longer able to reproduce the issue with this fix in place. Additionally, they've not been able to reproduce
 * the issue on 6.1.3 devices, or had any reports of the issue occuring on 6.1.3 devices.
 */
plcrash_error_t plcrash_async_mobject_init (plcrash_async_mobject_t *mobj, mach_port_t task, pl_vm_address_t task_addr, pl_vm_size_t length, bool require_full) {
    kern_return_t kt;

    /* Compute the total required page size. */
    pl_vm_address_t page_addr = mach_vm_trunc_page(task_addr);
    pl_vm_size_t page_size = mach_vm_round_page(length + (task_addr - page_addr));


    /* Create a reference to the target pages. The returned entry may be smaller than the requested length. */
    memory_object_size_t entry_length = page_size;
    mach_port_t mem_handle;
    kt = mach_make_memory_entry_64(task, &entry_length, page_addr, VM_PROT_READ, &mem_handle, MACH_PORT_NULL);
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

    /* Set initial start address for VM_FLAGS_ANYWHERE; vm_map() will search for the next unused region. */
    mobj->vm_address = 0x0;

    /* Map the pages into our local task */
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

    /* Clean up the now-unused reference */
    kt = mach_port_mod_refs(mach_task_self(), mem_handle, MACH_PORT_RIGHT_SEND, -1);
    if (kt != KERN_SUCCESS) {
        PLCF_DEBUG("mach_port_mod_refs(-1) failed: %d", kt);
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
}

/**
 * Return the base (target process relative) address for this mapping.
 *
 * @param mobj An initialized memory object.
 */
pl_vm_address_t plcrash_async_mobject_base_address (plcrash_async_mobject_t *mobj) {
    return mobj->task_address;
}

/**
 * Verify that @a length bytes starting at local @a address is within @a mobj's mapped range.
 *
 * @param mobj An initialized memory object.
 * @param address An address within the current task's memory space.
 * @param length The number of bytes that should be readable at @a address.
 */
bool plcrash_async_mobject_verify_local_pointer (plcrash_async_mobject_t *mobj, uintptr_t address, pl_vm_size_t offset, size_t length) {
    /* Verify that the offset value won't overrun */
    if (UINTPTR_MAX - offset < address)
        return false;

    /* Adjust the address using the verified offset */
    address += offset;

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
void *plcrash_async_mobject_remap_address (plcrash_async_mobject_t *mobj, pl_vm_address_t address, pl_vm_size_t offset, size_t length) {
    /* Map into our memory space */
    pl_vm_address_t remapped = address - mobj->vm_slide;

    if (!plcrash_async_mobject_verify_local_pointer(mobj, (uintptr_t) remapped, offset, length))
        return NULL;

    return (void *) remapped + offset;
}

/**
 * Free the memory mapping.
 *
 * @note Unlike most free() functions in this API, this function is async-safe.
 */
void plcrash_async_mobject_free (plcrash_async_mobject_t *mobj) {
    kern_return_t kt;
    if ((kt = vm_deallocate(mach_task_self(), mobj->address, mobj->length)) != KERN_SUCCESS)
        PLCF_DEBUG("vm_deallocate() failure: %d", kt);
}

/**
 * @} plcrash_async
 */