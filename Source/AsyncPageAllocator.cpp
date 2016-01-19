/*
 * Author: Landon Fuller <landonf@plausible.coop>
 *
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

#include "AsyncPageAllocator.hpp"
#include "AsyncAllocator.hpp"

#include "PLCrashAsync.h"

#include <libkern/OSAtomic.h>

PLCR_CPP_BEGIN_ASYNC_NS

/**
 * @internal
 * Internal contructor used when creating a new instance.
 *
 * @param base_page The base address of all allocated pages, including the leading guard page, if any.
 * @param total_size The total number of bytes allocated at base_page, including all guard pages.
 * @param usable_address The first address within the page allocation that may be used for user data.
 * @param usable_size The number of writable pages allocated at @a usable_page (ie, total_size, minus the guard pages).
 */
AsyncPageAllocator::AsyncPageAllocator (vm_address_t base_page, vm_size_t total_size, vm_address_t usable_address, vm_size_t usable_size) :
    _base_page(base_page),
    _total_size(total_size),
    _usable_address(usable_address),
    _usable_size(usable_size)
{
    /* assert sane parameters */
    PLCF_ASSERT(base_page <= usable_address);
    PLCF_ASSERT(usable_address <= base_page + total_size);
    PLCF_ASSERT(usable_address + usable_size <= base_page + total_size);
}

AsyncPageAllocator::~AsyncPageAllocator () {
    PLCF_ASSERT(_base_page != 0x0);
    
    /* Clean up our backing allocation. */
    kern_return_t kr = vm_deallocate(mach_task_self(), _base_page, _total_size);
    if (kr != KERN_SUCCESS) {
        /* This should really never fail ... */
        PLCF_DEBUG("[AsyncPageAllocator] vm_deallocate() failure: %d", kr);
    }
}

/* Deallocation of the allocator's memory is handled by the destructor itself; there's nothing for us to do. */
void AsyncPageAllocator::operator delete (void *ptr, size_t size) {}

/**
 * Create a new allocator instance, returning the pointer to the allocator in @a allocator on success. It is the caller's
 * responsibility to free this allocator (which will in turn free any memory allocated by the allocator) via `delete`.
 *
 * The allocator will be allocated within the same mapping, ensuring that the allocator metadata is
 * itself guarded.
 *
 * @param allocator On success, will contain a pointer to the newly created allocator.
 * @param size The size in bytes to be allocated; this will be rounded up to the nearest page size and must not be zero.
 * @param options The AsyncAllocatorOption flags to be used for this allocator.
 *
 * @return On success, returns PLCRASH_ESUCCESS. On failure, one of the plcrash_error_t error values will be returned.
 */
plcrash_error_t AsyncPageAllocator::Create (AsyncPageAllocator **allocator, size_t size, uint32_t options) {
    kern_return_t kt;
    
    /* Set defaults. */
    vm_size_t total_size = round_page(size);
    vm_size_t usable_size = total_size;
    
    /* Adjust total size to account for guard pages */
    if (options & GuardLowPage)
        total_size += PAGE_SIZE;
    
    if (options & GuardHighPage)
        total_size += PAGE_SIZE;
    
    /* Allocate memory pool */
    vm_address_t base_page;
    kt = vm_allocate(mach_task_self(), &base_page, total_size, VM_FLAGS_ANYWHERE);
    if (kt != KERN_SUCCESS) {
        PLCF_DEBUG("[AsyncPageAllocator] vm_allocate() failure: %d", kt);
        return PLCRASH_ENOMEM;
    }
    
    vm_address_t usable_page = base_page;
    
    /* Protect the guard pages */
    if (options & GuardLowPage) {
        usable_page += PAGE_SIZE;
        
        kt = vm_protect(mach_task_self(), base_page, PAGE_SIZE, false, VM_PROT_NONE);
        
        if (kt != KERN_SUCCESS) {
            PLCF_DEBUG("[AsyncPageAllocator] vm_protect() failure: %d", kt);
            
            kt = vm_deallocate(mach_task_self(), base_page, total_size);
            if (kt != KERN_SUCCESS)
                PLCF_DEBUG("[AsyncPageAllocator] vm_deallocate() failure: %d", kt);

            return PLCRASH_ENOMEM;
        }
    }
    
    if (options & GuardHighPage) {
        kt = vm_protect(mach_task_self(), base_page + total_size - PAGE_SIZE, PAGE_SIZE, false, VM_PROT_NONE);
        if (kt != KERN_SUCCESS) {
            PLCF_DEBUG("[AsyncPageAllocator] vm_protect() failure: %d", kt);
            
            kt = vm_deallocate(mach_task_self(), base_page, total_size);
            if (kt != KERN_SUCCESS)
                PLCF_DEBUG("[AsyncPageAllocator] vm_deallocate() failure: %d", kt);
            
            return PLCRASH_ENOMEM;
        }
    }
    
    
    /* Construct the allocator state in-place at the start of our allocated page.  */
    AsyncPageAllocator *a = ::new (placement_new_tag_t(), usable_page) AsyncPageAllocator(
        base_page,
        total_size,
        usable_page + sizeof(AsyncPageAllocator), /* Skip this initial allocation. */
        usable_size - sizeof(AsyncPageAllocator)
    );

    /* Provide the newly allocated (and self-referential) structure to the caller. */
    *allocator = a;
    return PLCRASH_ESUCCESS;
}

PLCR_CPP_END_ASYNC_NS
