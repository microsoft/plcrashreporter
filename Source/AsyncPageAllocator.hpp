/*
 * Author: Landon Fuller <landonf@plausible.coop>
 *
 * Copyright (c) 2013-2015 Plausible Labs Cooperative, Inc.
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

#ifndef PLCRASH_ASYNC_PAGE_ALLOCATOR_H
#define PLCRASH_ASYNC_PAGE_ALLOCATOR_H


#include <unistd.h>
#include <stdint.h>

#include "PLCrashAsync.h"
#include "PLCrashMacros.h"

/**
 * @internal
 * @ingroup plcrash_async
 *
 * @{
 */

PLCR_CPP_BEGIN_ASYNC_NS

/**
 * @internal
 *
 * An async-safe page-guarded allocator with page size allocation granularity.
 *
 * The allocator will allocate itself within the target pages, ensuring that its own metadata is
 * guarded.
 */
class AsyncPageAllocator {
public:
    /**
     * Initialization options for AsyncAllocator.
     */
    typedef enum {
        /**
         * Enable a low guard page. This will insert a PROT_NONE page prior to the
         * allocatable region, helping to ensure that a buffer overflow that occurs elsewhere
         * in the process will not overwrite the allocatable space.
         */
        GuardLowPage = 1 << 0,
        
        /**
         * Enable a high guard page. This will insert a PROT_NONE page after to the
         * allocatable region, helping to ensure that a buffer overflow (including a stack overflow)
         * will be immediately detected. */
        GuardHighPage = 1 << 1,
    } AsyncAllocatorOption;
    
    static plcrash_error_t Create (AsyncPageAllocator **allocator, size_t initial_size, uint32_t options);
    ~AsyncPageAllocator ();
    void operator delete (void *ptr, size_t size);
    
    /* An allocation instance may not be copied or moved; all access must be performed through the pointer returned
     * via Create(). */
    AsyncPageAllocator (const AsyncPageAllocator &other) = delete;
    AsyncPageAllocator (AsyncPageAllocator &&other) = delete;
    AsyncPageAllocator operator= (const AsyncPageAllocator &other) = delete;
    AsyncPageAllocator &operator= (AsyncPageAllocator &&other) = delete;
    
    /** Return the first address within the page allocation that may be used for user data. This address is not gauranteed to be aligned on any particular boundary. */
    vm_address_t usable_address () { return _usable_address; }
    
    /** Return the number of usable bytes at the address returned by usable_address(). */
    vm_address_t usable_size () { return _usable_size; }
    
private:
    AsyncPageAllocator (vm_address_t base_page, vm_size_t total_size, vm_address_t usable_address, vm_size_t usable_size);
    
    /** The address base of the allocation. */
    const vm_address_t _base_page;
    
    /** The total size of the allocation, including guard pages. */
    const vm_size_t _total_size;
    
    /** The first address within the page allocation that may be used for user data. */
    const vm_address_t _usable_address;

    /** The usable size of the allocation (ie, total size, minus guard pages and any internal AsyncPageAllocator state) */
    const vm_size_t _usable_size;
};
    
PLCR_CPP_END_ASYNC_NS

/**
 * @}
 */

#endif /* PLCRASH_ASYNC_PAGE_ALLOCATOR_H */
