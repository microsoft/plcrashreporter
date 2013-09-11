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

#include "PLCrashAsyncAllocator.h"
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

/**
 * @internal
 * An async-safe page-guarded and locking memory pool allocator. The allocator
 * will allocate itself within the target pages, ensuring that its own metadata is
 * guarded.
 */
struct plcrash_async_allocator {
    /** The address base of the allocation. */
    vm_address_t base_page;

    /** The total size of the allocation, including guard pages. */
    vm_size_t total_size;

    /** The base address of the first usable page of the allocation */
    vm_address_t usable_page;

    /** The usable size of the allocation (ie, total size, minus guard pages.) */
    vm_size_t usable_size;

    /** The next valid allocation address. */
    vm_address_t next_addr;

    /** PLCrashAsyncAllocatorOptions used when creating the allocator. */
    uint32_t options;
};

/**
 * Create and return a new allocator instance. The allocator will be allocated within the same mapping, ensuring
 * that the allocator metadata is itself guarded.
 *
 * @param allocator On success, will contain a reference to the newly created allocator.
 * @param size The total size of the allocated memory pool to be available for user allocations.
 * @param options PLCrashAsyncAllocatorOptions to be used for this allocator.
 *
 * @return On success, returns PLCRASH_ESUCCESS. On failure, one of the plcrash_error_t error values will be returned, and
 * any allocated resources will be discarded.
 */
plcrash_error_t plcrash_async_allocator_new (plcrash_async_allocator_t **allocator, size_t size, uint32_t options) {
    plcrash_async_allocator_t alloc;
    kern_return_t kt;

    /* Set defaults. */
    PLCF_ASSERT(SIZE_MAX - size > sizeof(plcrash_async_allocator_t));
    alloc.total_size = round_page(size + sizeof(plcrash_async_allocator_t));
    alloc.usable_size = alloc.total_size;
    alloc.options = options;
    fprintf(stderr, "allocated: %lu\n", (unsigned long) alloc.usable_size);

    /* Adjust total size to account for guard pages */
    if (options & PLCrashAsyncGuardLowPage)
        alloc.total_size += PAGE_SIZE;

    if (options & PLCrashAsyncGuardHighPage)
        alloc.total_size += PAGE_SIZE;
    


    /* Allocate memory pool */
    kt = vm_allocate(mach_task_self(), &alloc.base_page, alloc.total_size, VM_FLAGS_ANYWHERE);
    if (kt != KERN_SUCCESS) {
        PLCF_DEBUG("vm_allocate() failure: %d", kt);
        // Should we use more descriptive errors?
        return PLCRASH_ENOMEM;
    }

    alloc.next_addr = alloc.base_page;
    alloc.usable_page = alloc.base_page;


    /* Protect the guard pages */
    if (options & PLCrashAsyncGuardLowPage) {
        alloc.next_addr += PAGE_SIZE;
        alloc.usable_page += PAGE_SIZE;

        kt = vm_protect(mach_task_self(), alloc.base_page, PAGE_SIZE, false, VM_PROT_NONE);

        if (kt != KERN_SUCCESS) {
            PLCF_DEBUG("vm_protect() failure: %d", kt);
            vm_deallocate(mach_task_self(), alloc.base_page, alloc.total_size);
            return PLCRASH_ENOMEM;
        }
    }
    
    if (options & PLCrashAsyncGuardHighPage) {
        kt = vm_protect(mach_task_self(), alloc.base_page + (alloc.total_size - PAGE_SIZE), PAGE_SIZE, false, VM_PROT_NONE);
        if (kt != KERN_SUCCESS) {
            PLCF_DEBUG("vm_protect() failure: %d", kt);
            vm_deallocate(mach_task_self(), alloc.base_page, alloc.total_size);
            return PLCRASH_ENOMEM;
        }
    }


    /* Create a fake allocation for the allocator structure */
    alloc.next_addr = PL_ROUNDUP_ALIGN(alloc.next_addr + sizeof(alloc));
    *((plcrash_async_allocator_t *) alloc.usable_page) = alloc;
    

    /* Return the newly allocated (and self-referential) structure */
    *allocator = (void *) alloc.usable_page;
    return PLCRASH_ESUCCESS;
}

/**
 * Allocate @a size bytes from @a allocator. If insufficient space is available, an assertion will be thrown
 * unless @a no_assert is true, in which case NULL will be returned.
 *
 * @param allocator The allocator from which pages should be allocated
 * @param size The amount of memory to allocate, in bytes.
 * @param no_assert If true, NULL will be returned if insufficient space is available.
 */
void *plcrash_async_allocator_alloc (plcrash_async_allocator_t *allocator, size_t size, bool no_assert) {
    vm_address_t old_value;
    vm_address_t new_value;

    PLCF_ASSERT((sizeof(allocator->next_addr)) == sizeof(void *));

    /* Atomically bump the next_addr value */
    do {
        /* Verify available space */
        if (allocator->usable_size - (allocator->next_addr - allocator->usable_page) < size) {
            if (no_assert)
                return NULL;
            __builtin_trap();
        }

        old_value = allocator->next_addr;
        new_value = PL_ROUNDUP_ALIGN(allocator->next_addr + size);
    } while(!OSAtomicCompareAndSwapPtrBarrier((void *) old_value, (void *) new_value, (void **) &allocator->next_addr));

    return (void *) old_value;
}
