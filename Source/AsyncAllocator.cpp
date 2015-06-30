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

#include "AsyncAllocator.hpp"
#include "PLCrashAsync.h"

#include "AsyncPageAllocator.hpp"

/* Minimum useful allocation size; we won't bother to split a free block if the new free block would be
 * less than this size. */
#define PL_MINIMUM_USEFUL_FREEBLOCK_SIZE (plcrash::async::AsyncAllocator::round_align(sizeof(control_block)) * 2)

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

PLCR_CPP_BEGIN_ASYNC_NS

/**
 * @internal
 * Internal contructor used when creating a new instance.
 *
 * @param pageAllocator The initial set of pages to be used as our allocation pool. The allocator will claim ownership of this pointer.
 * @param initial_size The initial requested pool size.
 * @param first_block_address The naturally aligned address of the initial free block.
 * @param first_block_size The size in bytes of the initial free block.
 */
AsyncAllocator::AsyncAllocator (AsyncPageAllocator *pageAllocator, size_t initial_size, vm_address_t first_block_address, vm_size_t first_block_size) :
    _initial_size(initial_size),
    _initial_page_control(pageAllocator),
    _pageControls(&_initial_page_control),
    _free_list(NULL)
{
    /* Construct the first free list entry in-place, covering all remaining unallocated data */
    _free_list = new (placement_new_tag_t(), first_block_address) control_block(this, NULL, first_block_size);
    _free_list->_next = _free_list;
    
    _expected_unleaked_free_bytes = first_block_size;
}

AsyncAllocator::~AsyncAllocator () {
    PLCF_ASSERT(_pageControls != NULL);
    
    /* Check for leaks */
    if (_expected_unleaked_free_bytes != debug_bytes_free())
        PLCF_DEBUG("WARNING! Leaked %zd bytes in allocator %p", (ssize_t) (_expected_unleaked_free_bytes - debug_bytes_free()), this);
    
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
    
/**
 * @internal
 *
 * Grow the backing pool. This <em>must</em> be called with _lock held.
 *
 * @param required The minimum number of additional bytes required.
 */
plcrash_error_t AsyncAllocator::grow (vm_size_t required) {
    PLCF_ASSERT(!_lock.tryLock());

    plcrash_error_t err;
    
    /* While vm_allocate is generally async-safe, it is not gauranteed to be so. Ideally this allocator will be initialized once without
     * sufficient free space for all crash-time allocation operations. */
    PLCF_DEBUG("WARNING: Growing the AsyncAllocator free list via vm_allocate(). Increasing the initial size of this allocator is recommended.");
    
    /* Try allocating a new page pool */
    AsyncPageAllocator *newPages;
    err = AsyncPageAllocator::Create(&newPages, _initial_size + required, AsyncPageAllocator::GuardLowPage | AsyncPageAllocator::GuardHighPage);
    if (err != PLCRASH_ESUCCESS) {
        PLCF_DEBUG("AsyncPageAllocator::Create() failed while attempting to grow the pool: %d", err);
        
        _lock.unlock();
        return err;
    }

    /* Calculate the first usable address at which we can construct our page control. This must be aligned to natural_alignment(). */
    vm_address_t aligned_address = round_align(newPages->usable_address());
    vm_size_t aligned_size = newPages->usable_size() - (aligned_address - newPages->usable_address());
    
    /* Calculate the first usable free block past our page control. This must also be naturally aligned. */
    vm_address_t free_block_address = round_align(aligned_address + sizeof(page_control_block));
    vm_size_t free_block_size = aligned_size - (free_block_address - aligned_address);
    
    /* Construct our page control within the newly allocated pages and add to our PCB list. */
    page_control_block *pcb = new (placement_new_tag_t(), aligned_address) page_control_block(newPages, _pageControls);
    _pageControls = pcb;

    /* Construct the first free list entry in-place, covering all remaining unallocated data */
    control_block *new_block = new (placement_new_tag_t(), free_block_address) control_block(this, NULL, free_block_size);
    
    /* Update leak detection state */
    _expected_unleaked_free_bytes += free_block_size;
    
    /* Use the deallocation machinery to insert the new block into the free list, while maintaining sorting/coalescing */
    _lock.unlock(); /* Avoid deadlock */
    dealloc((void *) new_block->data());
    _lock.lock();

    return PLCRASH_ESUCCESS;
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
    err = AsyncPageAllocator::Create(&pageAllocator, initial_size + round_align(sizeof(page_control_block)), AsyncPageAllocator::GuardHighPage | AsyncPageAllocator::GuardLowPage);
    if (err != PLCRASH_ESUCCESS) {
        PLCF_DEBUG("AsyncPageAllocator::Create() failed: %d", err);
        return err;
    }
    
    /* Calculate the first usable address at which we can construct our AsyncAllocator. This must be aligned to natural_alignment(). */
    vm_address_t aligned_address = round_align(pageAllocator->usable_address());
    vm_size_t aligned_size = pageAllocator->usable_size() - (aligned_address - pageAllocator->usable_address());
    
    /* Calculate the first usable free block past our AsyncAllocator instance. This must also be naturally aligned. The block size must also be aligned
     * to the natural alignment; this should always be true when our backing pages are (by definition) paged align. */
    vm_address_t free_block_address = round_align(aligned_address + sizeof(AsyncAllocator));
    vm_size_t free_block_size = trunc_align(aligned_size - (free_block_address - aligned_address));

    /* Construct the allocator state in-place at the start of our allocated page.  */
    AsyncAllocator *a = new (placement_new_tag_t(), aligned_address) AsyncAllocator(pageAllocator, initial_size, free_block_address, free_block_size);
    
    /* Provide the newly allocated (and self-referential) structure to the caller. */
    *allocator = a;
    return PLCRASH_ESUCCESS;
}
    

/** Return the initial address of this entry. */
vm_address_t AsyncAllocator::control_block::head () { return (vm_address_t) this; }

/** Return the data address of this entry. */
vm_address_t AsyncAllocator::control_block::data () { return round_align(head() + sizeof(*this)); }

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
    plcrash_error_t err;
    
    /* Sanity check that adding size to sizeof(control_block) won't overflow */
    if (SIZE_MAX - size < round_align(sizeof(control_block)))
        return PLCRASH_ENOMEM;
    
    /* Compute the total number of bytes our allocation will need -- we have to lead with a control block. */
    size_t new_block_size = round_align(round_align(sizeof(control_block)) + size);
    
    /* Acquire our state lock */
    _lock.lock();
    
    /* If our pool has been exhausted, try to allocate additional pages from the OS. */
    if (_free_list == NULL) {
        if ((err = grow(size)) != PLCRASH_ESUCCESS) {
            PLCF_DEBUG("Failed to grow the free list: %d", err);
            
            _lock.unlock();
            return PLCRASH_ENOMEM;
        }
        
        PLCF_ASSERT(_free_list != NULL);
    }

    /* Find a free block with sufficient space for our allocation. */
retry:
    control_block *prev_cb = _free_list;
    control_block *start_cb = prev_cb;
    for (control_block *cb = _free_list;; prev_cb = cb, cb = cb->_next) {
        /* Sanity check; blocks must be owned by this allocator */
        PLCF_ASSERT(cb->_allocator == this);

        /* Blocks must always be properly aligned! */
        PLCF_ASSERT(cb->_size == round_align(cb->_size));

        /* Insufficient space; we need to keep looking. */
        if (cb->_size < new_block_size) {
            /* If we've iterated the entire free list, there isn't sufficient memory available. We need to allocate
             * additional pages for our pool */
            if (cb->_next == start_cb) {
                /* Try to grow */
                err = grow(size);
                if (err != PLCRASH_ESUCCESS) {
                    PLCF_DEBUG("Failed to grow the free list: %d", err);

                    _lock.unlock();
                    return PLCRASH_ENOMEM;
                }
                
                /* Restart our search */
                goto retry;
            }

            continue;
        }
        
        /* The block is large enough, but not so much larger that we should split it into two blocks. We can
         * use the block directly and remove it from the free list. */
        if (cb->_size >= new_block_size && (cb->_size - new_block_size) < PL_MINIMUM_USEFUL_FREEBLOCK_SIZE) {
            /* Remove the current block */
            prev_cb->_next = cb->_next;
            
            /* Update the free pointer. If this is the only block in the free list, our pool is exhausted (and we'll have to grow the pool on the next call) */
            if (prev_cb == cb) {
                _free_list = NULL;
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
        control_block *split_cb = new (placement_new_tag_t(), (((vm_address_t) cb) + cb->_size)) control_block(this, NULL, new_block_size);
        
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
 * Return the AsyncAllocator used to allocate @a ptr.
 *
 * @param ptr Memory allocated by an AsyncAllocator instance.
 */
AsyncAllocator *AsyncAllocator::allocator (void *ptr) {
    /* Fetch the control block */
    control_block *block = (control_block *) ((vm_address_t) ptr - round_align(sizeof(control_block)));
    
    PLCF_ASSERT(block->_allocator != nullptr);
    return block->_allocator;
}


/**
 * Deallocate the memory associated with @a ptr.
 *
 * @param ptr A pointer previously returned from alloc(), for which all associated memory will be deallocated.
 */
void AsyncAllocator::dealloc (void *ptr) {
    /* Fetch the control block */
    control_block *freeblock = (control_block *) ((vm_address_t) ptr - round_align(sizeof(control_block)));
    
    /* Blocks must be owned by this allocator */
    PLCF_ASSERT(freeblock->_allocator == this);
    
    /* Allocated blocks must have a NULL next value */
    PLCF_ASSERT(freeblock->_next == NULL);
    
    /* Acquire our state lock */
    _lock.lock();
    
    /* If the free list is empty, we can simply re-initialize it without worrying about sorting. */
    if (_free_list == NULL) {
        _free_list = freeblock;
        freeblock->_next = freeblock;
        
        _lock.unlock();
        return;
    }

    /*
     * Find the insertion point at which freeblock will be inserted as the next free block. We walk the list
     * until we either:
     * 1) Find two blocks that we'll fit between, or
     * 2) Hit the final node of the cyclic list, at which point high addresses cycle back to low addresses.
     */
    control_block *parent = _free_list;
    control_block *start = _free_list;
    do {
        parent = parent->_next;
    } while (freeblock > parent && parent->_next != start);
    
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

PLCR_CPP_END_ASYNC_NS

/**
 * @}
 */
