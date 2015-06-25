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

#include "AsyncAllocator.hpp"
#include "PLCrashAsync.h"

#include <libkern/OSAtomic.h>

/* These assume 16-byte malloc() alignment, which is true for just about everything */
#if defined(__arm__) || defined(__arm64__) || defined(__i386__) || defined(__x86_64__)
#define PL_NATURAL_ALIGNMENT 16
#else
#error Define required alignment
#endif

/* Macros to handling rounding to the nearest valid allocation alignment */
#define PL_ROUNDDOWN_ALIGN(x)   ((x) & (~(PL_NATURAL_ALIGNMENT - 1)))
#define PL_ROUNDUP_ALIGN(x)     PL_ROUNDDOWN_ALIGN((x) + (PL_NATURAL_ALIGNMENT - 1))

namespace plcrash { namespace async {
    /* Tag parameter required for our custom placement new operators defined below */
    struct internal_placement_new_tag_t {};
}}

/* Custom placement new alternatives that do not depend on the c++ stdlib, and use a type tag parameter that
 * will prevent symbol collision. */
void *operator new (size_t, const plcrash::async::internal_placement_new_tag_t &, vm_address_t p) { return (void *) p; };
void *operator new[] (size_t, const plcrash::async::internal_placement_new_tag_t &, vm_address_t p) { return (void *) p; };

namespace plcrash { namespace async {
    
/**
 * @internal
 * Internal contructor used when creating a new instance.
 */
AsyncAllocator::AsyncAllocator (vm_address_t base_page, vm_size_t total_size, vm_address_t usable_page, vm_size_t usable_size, vm_address_t next_addr) :
    _base_page(base_page),
    _total_size(total_size),
    _usable_page(usable_page),
    _usable_size(usable_size),
    _next_addr(PL_ROUNDUP_ALIGN(next_addr)) /* MUST be kept aligned */
{
}

AsyncAllocator::~AsyncAllocator () {
    PLCF_ASSERT(_base_page != 0x0);
    
    /* Clean up our backing allocation. */
    kern_return_t kr = vm_deallocate(mach_task_self(), _base_page, _total_size);
    if (kr != KERN_SUCCESS) {
        /* This should really never fail ... */
        PLCF_DEBUG("[AsyncAllocator] vm_deallocate() failure: %d", kr);
    }
}

/* Deallocation of the allocator's memory is handled by the destructor itself; there's nothing for us to do. */
void AsyncAllocator::operator delete (void *ptr, size_t size) {}

/**
 * Create a new allocator instance, returning the pointer to the allocator in @a allocator on success. It is the caller's
 * responsibility to free this allocator (which will in turn free any memory allocated by the allocator) via `delete`.
 *
 * The allocator will be allocated within the same mapping, ensuring that the allocator metadata is
 * itself guarded.
 *
 * @param allocator On success, will contain a pointer to the newly created allocator.
 * @param initial_size The initial size of the allocated memory pool to be available for user allocations. This pool
 * will automatically grow as required.
 * @param options The AsyncAllocatorOption flags to be used for this allocator.
 *
 * @return On success, returns PLCRASH_ESUCCESS. On failure, one of the plcrash_error_t error values will be returned.
 */
plcrash_error_t AsyncAllocator::Create (AsyncAllocator **allocator, size_t initial_size, uint32_t options) {
    kern_return_t kt;
    
    /* Set defaults. */
    PLCF_ASSERT(SIZE_MAX - initial_size > sizeof(AsyncAllocator));
    vm_size_t total_size = round_page(initial_size + sizeof(AsyncAllocator));
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
        PLCF_DEBUG("[AsyncAllocator] vm_allocate() failure: %d", kt);
        return PLCRASH_ENOMEM;
    }
    
    vm_address_t next_addr = base_page;
    vm_address_t usable_page = base_page;
    
    
    /* Protect the guard pages */
    if (options & GuardLowPage) {
        next_addr += PAGE_SIZE;
        usable_page += PAGE_SIZE;
        
        kt = vm_protect(mach_task_self(), base_page, PAGE_SIZE, false, VM_PROT_NONE);
        
        if (kt != KERN_SUCCESS) {
            PLCF_DEBUG("[AsyncAllocator] vm_protect() failure: %d", kt);
            vm_deallocate(mach_task_self(), base_page, total_size);
            return PLCRASH_ENOMEM;
        }
    }
    
    if (options & GuardHighPage) {
        kt = vm_protect(mach_task_self(), base_page + usable_size, PAGE_SIZE, false, VM_PROT_NONE);
        if (kt != KERN_SUCCESS) {
            PLCF_DEBUG("[AsyncAllocator] vm_protect() failure: %d", kt);
            vm_deallocate(mach_task_self(), base_page, total_size);
            return PLCRASH_ENOMEM;
        }
    }
    
    
    /* Construct the allocator state in-place at the start of our allocated page.  */
    AsyncAllocator *a = ::new (internal_placement_new_tag_t(), usable_page) AsyncAllocator(
        base_page,
        total_size,
        usable_page,
        usable_size,
        next_addr + sizeof(AsyncAllocator) /* Remove this initial allocation from the 'free' list */
    );
    
    
    /* Provide the newly allocated (and self-referential) structure to the caller. */
    *allocator = a;
    return PLCRASH_ESUCCESS;
}

/**
 * Attempt to allocate @a size bytes, returning a pointer to the allocation in @a allocated on success. If insufficient space
 * is available, PLCRASH_ENOMEM will be returned.
 *
 * @param allocated On success, will contain a pointer to the allocated memory.
 * @param size The number of bytes to allocate.
 *
 * @return On success, returns PLCRASH_ESUCCESS. If additional bytes can not be allocated, PLCRASH_ENOMEM will be returned.
 */
plcrash_error_t AsyncAllocator::alloc (void **allocated, size_t size) {
    vm_address_t old_value;
    vm_address_t new_value;
    
    /* This /must/ be kept aligned */
    PLCF_ASSERT(_next_addr == PL_ROUNDUP_ALIGN(_next_addr));
    
    /* Sanity check to verify that our use of OSAtomicCompareAndSwapPtrBarrier on a vm_address_t is correct */
    PLCF_ASSERT((sizeof(_next_addr)) == sizeof(void *));
    
    /* Atomically bump the next_addr value */
    do {
        /* Verify available space */
        if (_usable_size - (_next_addr - _usable_page) < size) {
            // TODO - we should grow our pool on-demand
            return PLCRASH_ENOMEM;
        }
        
        old_value = _next_addr;
        new_value = PL_ROUNDUP_ALIGN(_next_addr + size);
    } while(!OSAtomicCompareAndSwapPtrBarrier((void *) old_value, (void *) new_value, (void **) &_next_addr));
    
    *allocated = (void *) old_value;
    return PLCRASH_ESUCCESS;
}

/**
 * Deallocate the memory associated with @a ptr.
 *
 * @param ptr A pointer previously returned from alloc(), for which all associated memory will be deallocated.
 */
void AsyncAllocator::dealloc (void *ptr) {
    // TODO - we'd need a real allocator
}    

}}
