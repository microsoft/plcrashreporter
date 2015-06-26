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
#include "AsyncPageAllocator.hpp"

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

/* The number of bytes at the start of an allocation that must be preserved for the control block */
#define PL_CONTROLBLOCK_HEADER_BYTES PL_ROUNDUP_ALIGN(sizeof(control_block))

/* Minimum useful allocation size; we won't bother to split a free block if the new free block would be
 * less than this size. */
#define PL_MINIMUM_USEFUL_FREEBLOCK_SIZE (PL_ROUNDUP_ALIGN(sizeof(control_block)) * 2)

/**
 * @internal
 * @ingroup plcrash_async
 *
 * @{
 */

/**
 * An implementation of the C++11 'placement new' operator that does not depend on the C++ standard library.
 *
 * @param tag A tag parameter required to use the custom operator. The use of a tag parameter prevents symbol
 * collision with any other placement new overrides.
 * @param p The address at which the instance should be constructed.
 */
void *operator new (size_t, const plcrash::async::placement_new_tag_t &tag, vm_address_t p) { return (void *) p; };

/**
 * An implementation of the C++11 array 'placement new' operator that does not depend on the C++ standard library.
 *
 * @param tag A tag parameter required to use the custom operator. The use of a tag parameter prevents symbol
 * collision with any other placement new overrides.
 * @param p The address at which the instances should be constructed.
 */
void *operator new[] (size_t, const plcrash::async::placement_new_tag_t &tag, vm_address_t p) { return (void *) p; };

namespace plcrash { namespace async {

/**
 * @internal
 * Internal contructor used when creating a new instance.
 *
 * @param pageAllocator The initial set of pages to be used as our allocation pool. The allocator will claim ownership of this pointer.
 * @param first_block_address The naturally aligned address of the initial free block.
 * @param first_block_size The size in bytes of the initial free block.
 */
AsyncAllocator::AsyncAllocator (AsyncPageAllocator *pageAllocator, vm_address_t first_block_address, vm_size_t first_block_size) :
    _pageControls(NULL),
    _free_list(NULL)
{
    PLCF_ASSERT(first_block_size >= sizeof(page_control_block));
    
    /* Construct our first page control */
    _pageControls = new (placement_new_tag_t(), first_block_address) page_control_block(pageAllocator, NULL);
    
    /* Calculate the remaining unallocated space */
    vm_address_t free_block = PL_ROUNDUP_ALIGN(first_block_address + sizeof(page_control_block));
    vm_size_t free_size = first_block_size - (free_block - first_block_address);

    /* Construct the first free list entry in-place, covering all remaining unallocated data */
    _free_list = new (placement_new_tag_t(), free_block) control_block(NULL, free_size);
    _free_list->_next = _free_list;
}

AsyncAllocator::~AsyncAllocator () {
    PLCF_ASSERT(_pageControls != NULL);
    
    /* Clean up our backing allocations. Note that we copy out the next page control, as deallocating
     * the previous page will also deallocate its control. */
    page_control_block *next = NULL;
    for (page_control_block *pageControl = _pageControls; pageControl != NULL; pageControl = next) {
        /* Save a reference to the next control */
        next = pageControl->_next;
        
        /* Deallocate the page allocator. This will deallocate the pageControl instance, and if this is the first set of
         * pages, it will deallocate this AsyncAllocator instance. */
        delete pageControl->_pageAllocator;
    }
}

/* Deallocation of the allocator's memory is handled by the destructor itself; there's nothing for us to do. */
void AsyncAllocator::operator delete (void *ptr, size_t size) {}

/**
 * Create a new allocator instance, returning the pointer to the allocator in @a allocator on success. It is the caller's
 * responsibility to free this allocator (which will in turn free any memory allocated by the allocator) via `delete`.
 *
 * The allocator will be allocated within its own guarded memory pool, ensuring that the allocator metadata is
 * itself guarded.
 *
 * @param allocator On success, will contain a pointer to the newly created allocator.
 * @param initial_size The initial size of the allocated memory pool to be available for user allocations. This pool
 * will automatically grow as required.
 *
 * @return On success, returns PLCRASH_ESUCCESS. On failure, one of the plcrash_error_t error values will be returned.
 */
plcrash_error_t AsyncAllocator::Create (AsyncAllocator **allocator, size_t initial_size) {
    plcrash_error_t err;
    
    /* Allocate memory pool */
    AsyncPageAllocator *pageAllocator;
    err = AsyncPageAllocator::Create(&pageAllocator, initial_size + PL_ROUNDUP_ALIGN(sizeof(page_control_block)), AsyncPageAllocator::GuardHighPage | AsyncPageAllocator::GuardLowPage);
    if (err != PLCRASH_ESUCCESS) {
        PLCF_DEBUG("AsyncPageAllocator::Create() failed: %d", err);
        return err;
    }
    
    /* Calculate the first usable address at which we can construct our AsyncAllocator. This must be aligned as per PL_NATURAL_ALIGNMENT. */
    vm_address_t aligned_address = PL_ROUNDUP_ALIGN(pageAllocator->usable_address());
    vm_size_t aligned_size = pageAllocator->usable_size() - (aligned_address - pageAllocator->usable_address());
    
    /* Calculate the first usable free block past our AsyncAllocator instance. This must also be naturally aligned. */
    vm_address_t free_block_address = PL_ROUNDUP_ALIGN(aligned_address + sizeof(AsyncAllocator));
    vm_size_t free_block_size = aligned_size - (free_block_address - aligned_address);

    /* Construct the allocator state in-place at the start of our allocated page.  */
    AsyncAllocator *a = ::new (placement_new_tag_t(), aligned_address) AsyncAllocator(pageAllocator, free_block_address, free_block_size);
    
    /* Provide the newly allocated (and self-referential) structure to the caller. */
    *allocator = a;
    return PLCRASH_ESUCCESS;
}
    

/** Return the initial address of this entry. */
vm_address_t AsyncAllocator::control_block::head () { return (vm_address_t) this; }

/** Return the data address of this entry. */
vm_address_t AsyncAllocator::control_block::data () { return PL_ROUNDUP_ALIGN(head() + sizeof(*this)); }

/** Return the tail address of this entry. */
vm_address_t AsyncAllocator::control_block::tail () { return head() + _size; }


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
    /* Sanity check that adding size to sizeof(control_block) won't overflow */
    if (SIZE_MAX - size < PL_CONTROLBLOCK_HEADER_BYTES)
        return PLCRASH_ENOMEM;
    
    /* Compute the total number of bytes our allocation will need -- we have to lead with a control block. */
    size_t new_block_size = PL_ROUNDUP_ALIGN(PL_CONTROLBLOCK_HEADER_BYTES + size);
    
    /* Acquire our state lock */
    _lock.lock();

    /* Find a free block with sufficient space for our allocation. */
    control_block *prev_cb = _free_list;
    for (control_block *cb = _free_list;; prev_cb = cb, cb = cb->_next) {
        /* Blocks must always be properly aligned! */
        PLCF_ASSERT(cb->_size == PL_ROUNDUP_ALIGN(cb->_size));

        /* Insufficient space; we need to keep looking. */
        if (cb->_size < new_block_size) {
            // TODO - we need to terminate when the free list comes up completely empty.
            continue;
        }
        
        /* The block is large enough, but not so much larger that we should split it into two blocks. We can
         * use the block directly and remove it from the free list. */
        if (cb->_size >= new_block_size && (cb->_size - new_block_size) < PL_MINIMUM_USEFUL_FREEBLOCK_SIZE) {
            /* Remove the current block */
            prev_cb->_next = cb->_next;
            
            /* Update the free pointer. If this is the only block in the free list, we've now run out of memory. */
            if (prev_cb == cb) {
                // TODO - allocate additional pages from the OS
                __builtin_trap();
            } else {
                _free_list = prev_cb;
            }
            
            /* Reset the next pointer to NULL; required for allocated blocks */
            cb->_next = NULL;
            
            /* Return our result */
            *allocated = (void *) cb->data();
            
            _lock.unlock();
            return PLCRASH_ESUCCESS;
        }
        
        /* Otherwise, the block is big enough to be split into two blocks. We reserve the latter half of the free block for our
         * new allocation. */
        cb->_size -= new_block_size;
        
        /* We have to create a control block for the new allocation. */
        control_block *split_cb = new (placement_new_tag_t(), (((vm_address_t) cb) + cb->_size)) control_block(NULL, new_block_size);
        
        /* Lastly, we can return the user's allocation address (which falls just past the control block */
        *allocated = (void *) split_cb->data();
        
        _lock.unlock();
        return PLCRASH_ESUCCESS;
    }

    /* If we got here, we couldn't find a free block, and we couldn't grow our pool. */
    _lock.unlock();
    return PLCRASH_ENOMEM;
}

/**
 * Deallocate the memory associated with @a ptr.
 *
 * @param ptr A pointer previously returned from alloc(), for which all associated memory will be deallocated.
 */
void AsyncAllocator::dealloc (void *ptr) {
    /* Fetch the control block */
    control_block *freeblock = ((control_block *) ptr) - 1;
    
    /* Allocated blocks must have a NULL next value */
    PLCF_ASSERT(freeblock->_next == NULL);
    
    /* Acquire our state lock */
    _lock.lock();
    
    /*
     * Find the insertion point at which freeblock will be inserted as the next free block. We walk the list
     * until we either:
     * 1) Find two blocks that we'll fit between, or
     * 2) Hit the final node of the cyclic list, at which point high addresses cycle back to low addresses.
     */
    control_block *parent;
    for (parent = _free_list; freeblock > parent->_next && parent > parent->_next; parent = parent->_next) {};
    
    /* Try to coalesce with the next node. */
    if (freeblock->tail() == parent->_next->head()) {
        freeblock->_size += parent->_next->_size;
        freeblock->_next = parent->_next->_next;
    } else {
        freeblock->_next = parent->_next;
    }
    
    /* Try to coalesce with the previous node. */
    if (parent->tail() == freeblock->head()) {
        parent->_size += freeblock->_size;
        parent->_next = freeblock->_next;
    } else {
        parent->_next = freeblock;
    }
    
    /* Update the initial search point of our free list */
    _free_list = parent;
    
    /* Release our state lock */
    _lock.unlock();
}

}}

/**
 * @}
 */
