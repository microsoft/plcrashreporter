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

#ifndef PLCRASH_ASYNC_ALLOCATOR_H
#define PLCRASH_ASYNC_ALLOCATOR_H


#include <unistd.h>
#include <stdint.h>

#include "PLCrashAsync.h"
#include "SpinLock.hpp"

/**
 * @internal
 * @ingroup plcrash_async
 *
 * @{
 */

namespace plcrash { namespace async {
/**
 * Tag parameter required when invoking the async-safe placement new operators defined by
 * the PLCrashReporter async-safe allocator.
 */
struct placement_new_tag_t {};
}}

void *operator new (size_t, const plcrash::async::placement_new_tag_t &tag, vm_address_t p);
void *operator new[] (size_t, const plcrash::async::placement_new_tag_t &tag, vm_address_t p);

namespace plcrash { namespace async {

/* Forward declarations */
class AsyncAllocator;
class AsyncPageAllocator;

/**
 * @internal
 *
 * Provides new and delete operators that may only be used in combination with
 * an async-safe allocator.
 */
class AsyncAllocatable {
public:
    void *operator new (size_t size, AsyncAllocator &allocator);
    void *operator new[] (size_t size, AsyncAllocator &allocator);
    
    void operator delete (void *ptr, size_t size);
    void operator delete[] (void *ptr, size_t size);

private:
    /* Disallow non-async-safe allocation entirely. */
    void *operator new (size_t);
    void *operator new[] (size_t);
};


/**
 * @internal
 *
 * An async-safe memory allocator.
 *
 * The allocator will automatically insert PROT_NONE guard pages before and after any
 * allocated memory pools from which allocations are made, helping to ensure that a
 * buffer overflow that occurs elsewhere in the code will not overwrite allocations
 * within this allocator.
 */
class AsyncAllocator {
public:
    static plcrash_error_t Create (AsyncAllocator **allocator, size_t initial_size);
    ~AsyncAllocator ();
    void operator delete (void *ptr, size_t size);

    plcrash_error_t alloc (void **allocated, size_t size);
    void dealloc (void *ptr);
    
    /* An allocation instance may not be copied or moved; all access must be performed through the pointer returned
     * via Create(). */
    AsyncAllocator (const AsyncAllocator &other) = delete;
    AsyncAllocator (AsyncAllocator &&other) = delete;
    AsyncAllocator operator= (const AsyncAllocator &other) = delete;
    AsyncAllocator &operator= (AsyncAllocator &&other) = delete;

private:
    AsyncAllocator (AsyncPageAllocator *pageAllocator, vm_address_t first_block, vm_size_t first_block_size);
    
    /**
     * @internal
     *
     * A page pool header block that sits at the start of all page allocations and is used to
     * maintain a linked list of all allocated pools.
     */
    struct page_control_block {
        /* Construct a new page control block instance. */
        page_control_block (AsyncPageAllocator *pageAllocator, page_control_block *next) : _pageAllocator(pageAllocator), _next(next) {}
        
        /** A borrowed reference to the AsyncPageAllocator associated with this page. */
        AsyncPageAllocator *_pageAllocator;

        /** The next page control block, or NULL if this is the final control block. */
        page_control_block *_next;
    };
    
    /** Lock that must be held when operating on the non-const allocator state */
    SpinLock _lock;

    /** All backing page controls. */
    page_control_block *_pageControls;

    /**
     * @internal
     * 
     * A control block sits at the start of all allocations, and is used to form a circular
     * free list.
     */
    struct control_block {
        /* Construct a new control block instance */
        control_block (control_block *next, size_t size) : _next(next), _size(size) {}
        
        /** Pointer to next block in the free list, or NULL if this block has been allocated. */
        control_block *_next;
        
        /** Size of this block */
        size_t _size;
        
        vm_address_t head ();
        vm_address_t data ();
        vm_address_t tail ();
    };
    
    /**
     * Head of the circular free list, or NULL if memory has been exhausted and additional pages
     * could not be allocated.
     *
     * Note:
     * - The list is sorted in ascending order by address, but
     * - The list is also circular, the highest address entry will loop back to the lowest address entry.
     * - This pointer will not necessarily point to the lowest address as the "head" of the list.
     * - The 'next' element in a single element list will refer cyclically to itself.
     */
    control_block *_free_list;
};
    
}}

/**
 * @}
 */

#endif /* PLCRASH_ASYNC_ALLOCATOR_H */
