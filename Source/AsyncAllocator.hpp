/*
 * Author: Landon Fuller <landonf@plausible.coop>
 *
 * Copyright (c) 2013 - 2015 Plausible Labs Cooperative, Inc.
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
class AsyncPageAllocator;

/**
 * @internal
 *
 * An async-safe memory allocator.
 *
 * The allocator will automatically insert PROT_NONE guard pages before and after any
 * allocated memory pools from which allocations are made, helping to ensure that a
 * buffer overflow that occurs elsewhere in the process will not overwrite allocations
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
    
    /**
     * @internal
     *
     * Return the natural alignment to be used on this platform for all allocations.
     */
    static inline vm_address_t natural_alignment () {
        /* 16-byte natural alignment is true for just about everything */
#if defined(__arm__) || defined(__arm64__) || defined(__i386__) || defined(__x86_64__)
        return 16;
#else
#       error Define required alignment
#endif
    }
    
    /**
     * @internal
     *
     * Round @a value up to the nearest natural alignment boundary.
     *
     * @param value the value to be aligned.
     */
    static inline vm_address_t round_align (vm_address_t value) {
        return trunc_align((value) + (natural_alignment() - 1));
    }
    
    /**
     * @internal
     *
     * Truncate @a value up to the nearest natural alignment boundary.
     *
     * @param value the value to be aligned.
     */
    static inline vm_address_t trunc_align (vm_address_t value) {
        return ((value) & (~(natural_alignment() - 1)));
    }

#ifndef PLCF_ASYNCALLOCATOR_DEBUG
private:
#endif
    AsyncAllocator (AsyncPageAllocator *pageAllocator, size_t initial_size, vm_address_t first_block, vm_size_t first_block_size);
    
    plcrash_error_t grow (vm_size_t required);
    
    /**
     * @internal
     *
     * A page pool header block that sits at the start of all page allocations and is used to
     * maintain a linked list of all allocated pools.
     */
    struct page_control_block {
        /* Construct a new page control block instance. */
        page_control_block (AsyncPageAllocator *pageAllocator, page_control_block *next) : _pageAllocator(pageAllocator), _next(next) {}
        
        /* Construct a new page control block instance with a NULL `next` value. */
        page_control_block (AsyncPageAllocator *pageAllocator) : _pageAllocator(pageAllocator), _next(NULL) {}
        
        /** A borrowed reference to the AsyncPageAllocator associated with this page. */
        AsyncPageAllocator *_pageAllocator;

        /** The next page control block, or NULL if this is the final control block. */
        page_control_block *_next;
    };
    
    /** The initial requested size. */
    const size_t _initial_size;
    
    /** Lock that must be held when operating on the non-const allocator state */
    SpinLock _lock;
    
    /** Inline allocation for the first page control block; there is always at least one. */
    page_control_block _initial_page_control;

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
     * Head of the circular free list, or NULL if memory has been exhausted.
     *
     * Note:
     * - The list is sorted in ascending order by address, but
     * - The list is also circular, the highest address entry will loop back to the lowest address entry.
     * - This pointer will not necessarily point to the lowest address as the "head" of the list.
     * - The 'next' element in a single element list will refer cyclically to itself.
     */
    control_block *_free_list;
    
#ifdef PLCF_ASYNCALLOCATOR_DEBUG
    /**
     * Return the number of bytes consumed by all free list blocks.
     */
    vm_size_t debug_bytes_free () {
        vm_size_t bytes_free = 0;
        
        _lock.lock();
        control_block *first = _free_list;
        for (control_block *b = _free_list; b != NULL; b = b->_next) {
            bytes_free += b->_size;
            
            if (b->_next == first)
                break;
        }
        _lock.unlock();
        
        return bytes_free;
    }
#endif    
};
    
}}

/**
 * @}
 */

#endif /* PLCRASH_ASYNC_ALLOCATOR_H */
