/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008-2013 Plausible Labs Cooperative, Inc.
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

#import "SenTestCompat.h"

#import "AsyncPageAllocator.hpp"

#define PLCF_ASYNCALLOCATOR_DEBUG 1
#import "AsyncAllocator.hpp"

#import <mach/vm_region.h>

using namespace plcrash::async;

@interface PLCrashAsyncAllocatorTests : SenTestCase {
@private
}

@end

@implementation PLCrashAsyncAllocatorTests

/* Sanity-check the natural alignment declared by the allocator */
- (void) testNaturalAlignment {
    void *malloc_buffer = malloc(64);
    
    STAssertNotNULL(malloc_buffer, @"malloc() failed");
    STAssertTrue((uintptr_t) malloc_buffer % AsyncAllocator::natural_alignment() == 0, @"The buffer returned by malloc does not match our declared natural alignment");
    
    free(malloc_buffer);
}

/* Verify the initial allocation state; all the other tests below assume the assertions verified here. */
- (void) testInitialization {
    AsyncAllocator *allocator;
    STAssertEquals(PLCRASH_ESUCCESS, AsyncAllocator::Create(&allocator, PAGE_SIZE), @"Failed to construct allocator");
    
    /* Sanity check the page controls */
    STAssertNotNULL(allocator->_pageControls, @"No page controls were initialized");
    STAssertEquals(allocator->_pageControls, &allocator->_initial_page_control, @"The initial page control address is incorrect");
    STAssertNULL(allocator->_pageControls->_next, @"The initial page control has a non-NULL 'next' value");
    
    /* Fetch the backing page addresses */
    vm_address_t usable_address = allocator->_pageControls->_pageAllocator->usable_address();
    vm_size_t usable_size = allocator->_pageControls->_pageAllocator->usable_size();

    /* Calculate the address at which the allocator should have been placed, and the remaining number of bytes
     * that should be in the free list. */
    vm_address_t allocator_addr = AsyncAllocator::round_align(allocator->_pageControls->_pageAllocator->usable_address());
    STAssertEquals((vm_address_t) allocator, allocator_addr, @"The allocator was not placed at the first usable aligned address");

    vm_size_t free_size = usable_size - (allocator_addr - usable_address) - AsyncAllocator::round_align(sizeof(AsyncAllocator));

    /* Sanity check the free list */
    STAssertEquals(allocator->_free_list->_next, allocator->_free_list, @"The initial free list is not circular");
    STAssertEquals(allocator->_free_list->_size, free_size, @"The free size does not match the expected number of remaining bytes");
}

/* Test allocation of whole blocks. */
- (void) testWholeBlockAllocation {
    AsyncAllocator *allocator;
    STAssertEquals(PLCRASH_ESUCCESS, AsyncAllocator::Create(&allocator, PAGE_SIZE), @"Failed to construct allocator");

    /* Try to allocate all remaining data, triggering a whole-block assignment */
    vm_size_t available = allocator->_free_list->_size - AsyncAllocator::round_align(sizeof(AsyncAllocator::control_block));
    void *buffer;

    STAssertEquals(PLCRASH_ESUCCESS, allocator->alloc(&buffer, available), @"Allocation failed");
    
    /* Sanity check writability of the allocation; If this doesn't explode, we're good. */
    memset(buffer, 0, available);
    
    /* Verify that this left the the free list exhausted */
    STAssertNULL(allocator->_free_list, @"_free_list was not marked as exhausted");

    allocator->dealloc(buffer);
    delete allocator;
}

/* Test splitting of the free list into multiple blocks */
- (void) testBlockSplitting {
    AsyncAllocator *allocator;
    STAssertEquals(PLCRASH_ESUCCESS, AsyncAllocator::Create(&allocator, PAGE_SIZE), @"Failed to construct allocator");
    vm_size_t free_block_address = allocator->_free_list->head();
    vm_size_t free_block_size = allocator->_free_list->_size;
    
    /* Allocate a small part of the only available free block, triggering block splitting. */
    void *buffer;
    STAssertEquals(PLCRASH_ESUCCESS, allocator->alloc(&buffer, 16), @"Allocation failed");
    vm_size_t free_block_consumed = AsyncAllocator::round_align(sizeof(AsyncAllocator::control_block)) + 16;
    
    /* Sanity check writability of the allocation; If this doesn't explode, we're good. */
    memset(buffer, 0, 16);

    /* Verify that this left a single free block of the expected size */
    STAssertNotNULL(allocator->_free_list, @"_free_list was incorrectly marked as exhausted");
    STAssertEquals(free_block_address, allocator->_free_list->head(), @"The free block address changed; split at the beginning instead of the end?");
    STAssertEquals(free_block_size - free_block_consumed, allocator->_free_list->_size, @"Incorrect number of bytes remaining in the free block");
    
    /* Verify the block created for our allocation. */
    AsyncAllocator::control_block *block = (AsyncAllocator::control_block*) allocator->_free_list->tail();
    STAssertEquals(block->data(), (vm_address_t) buffer, @"The returned allocation pointer does not match our expected control block location");
    STAssertNULL(block->_next, @"The _next field was not set to NULL on an allocated block");
    STAssertEquals(block->_size, free_block_consumed, @"The allocated block's size is incorrect");
    
    allocator->dealloc(buffer);
    delete allocator;
}

/* Test coalescing and non-coalescing free list updates */
- (void) testDeallocFreeListUpdate {
    AsyncAllocator *allocator;
    STAssertEquals(PLCRASH_ESUCCESS, AsyncAllocator::Create(&allocator, 64), @"Failed to construct allocator");

    /* The full size of our allocated blocks */
    vm_size_t test_block_size = AsyncAllocator::round_align(sizeof(AsyncAllocator::control_block)) + 16;

    /* Save the original free size */
    vm_size_t orig_free_size = allocator->debug_bytes_free();

    /* Allocate a set of test blocks */
    void *high_buf;
    STAssertEquals(PLCRASH_ESUCCESS, allocator->alloc(&high_buf, 16), @"Allocation failed");
    AsyncAllocator::control_block *high_cb = (AsyncAllocator::control_block *) ((pl_vm_address_t) high_buf - AsyncAllocator::round_align(sizeof(AsyncAllocator::control_block)));

    void *mid_buf;
    STAssertEquals(PLCRASH_ESUCCESS, allocator->alloc(&mid_buf, 16), @"Allocation failed");
    AsyncAllocator::control_block *mid_cb = (AsyncAllocator::control_block *) ((pl_vm_address_t) mid_buf - AsyncAllocator::round_align(sizeof(AsyncAllocator::control_block)));

    void *low_buf;
    STAssertEquals(PLCRASH_ESUCCESS, allocator->alloc(&low_buf, 16), @"Allocation failed");
    AsyncAllocator::control_block *low_cb = (AsyncAllocator::control_block *) ((pl_vm_address_t) low_buf - AsyncAllocator::round_align(sizeof(AsyncAllocator::control_block)));


    /* Verify that the allocations were performed in the expected decreasing address order, at the expected addresses. */
    STAssertTrue(high_buf > mid_buf, @"");
    STAssertTrue(mid_buf > low_buf, @"");
    
    STAssertTrue(AsyncAllocator::round_align((vm_address_t)mid_buf + 16) == (vm_address_t) high_cb, @"Buffer was not allocated at expected location");
    STAssertTrue(AsyncAllocator::round_align((vm_address_t)low_buf + 16) == (vm_address_t) mid_cb, @"Buffer was not allocated at expected location");

    /* The free list should have one entry */
    AsyncAllocator::control_block *orig_free_list = allocator->_free_list;
    
    STAssertTrue(allocator->_free_list == allocator->_free_list->_next, @"The free list has more than one entry, or in invalid _next value");
    STAssertEquals(allocator->debug_bytes_free(), orig_free_size - (test_block_size * 3), @"The free list size does not match the expected size");

    
    /* Returning the middle buffer to the free list, verifying that the free list now contains two blocks: the
     * previous free block, and the newly deallocated block. */
    allocator->dealloc(mid_buf);
    
    STAssertEquals(allocator->_free_list, orig_free_list, @"The free list pointer wasn't updated to point at the freed block's parent");
    STAssertEquals(allocator->_free_list->_next, mid_cb, @"The parent block does not reference the newly freed block");
    STAssertEquals(allocator->_free_list->_next->_next, allocator->_free_list, @"The free list pointer doesn't cycle back around to the first block");
    
    STAssertEquals(mid_cb->_size, AsyncAllocator::round_align(sizeof(AsyncAllocator::control_block)) + 16, @"The deallocated block's size was corrupted");
    STAssertEquals(allocator->debug_bytes_free(), orig_free_size - (test_block_size * 2), @"The free list size does not match the expected size");


    /* Return the high buffer to the free list. It should be coalesced with the middle buffer, again leaving the free
     * list with two blocks */
    allocator->dealloc(high_buf);
    
    STAssertEquals(allocator->_free_list, mid_cb, @"The free list pointer wasn't updated to point at the freed block's parent");
    STAssertEquals(allocator->_free_list->_next, orig_free_list, @"The free list pointer doesn't cycle back around to the first block");
    STAssertEquals(allocator->_free_list->_next->_next, allocator->_free_list, @"The free list pointer doesn't cycle back around to the freed block");

    STAssertEquals(mid_cb->_size, (AsyncAllocator::round_align(sizeof(AsyncAllocator::control_block)) + 16) * 2, @"The coalesced mid_cb block's size was not updated");
    STAssertEquals(allocator->debug_bytes_free(), orig_free_size - (test_block_size), @"The free list size does not match the expected size");


    /* Return the low buffer to the free list. It should be coalesced with all remaining blocks, leaving the free list with one block */
    allocator->dealloc(low_buf);
    
    STAssertNotEquals(allocator->_free_list, low_cb, @"The low buffer should have been coalesced with all other free list items");
    STAssertEquals(allocator->_free_list, orig_free_list, @"The free list pointer wasn't updated to point at the freed block's parent");
    STAssertEquals(allocator->_free_list, allocator->_free_list->_next, @"The free list has more than one entry, or in invalid _next value");

    STAssertEquals(allocator->_free_list->_size, allocator->debug_bytes_free(), @"The free list does not match the calculated number of free bytes");
    STAssertEquals(allocator->_free_list->_size, orig_free_size, @"The free list size does not match the expected coalesced size");

    delete allocator;
}

/* Test automatic page allocation after exhausting all blocks */
- (void) testFreeListExhaustion {
    AsyncAllocator *allocator;
    STAssertEquals(PLCRASH_ESUCCESS, AsyncAllocator::Create(&allocator, PAGE_SIZE), @"Failed to construct allocator");
    
    /* Allocate all available data, triggering free list exhaustion */
    vm_size_t available = allocator->_free_list->_size - AsyncAllocator::round_align(sizeof(AsyncAllocator::control_block));
    void *buffer;
    STAssertEquals(PLCRASH_ESUCCESS, allocator->alloc(&buffer, available), @"Allocation failed");
    
    /* Verify that this left the the free list exhausted */
    STAssertNULL(allocator->_free_list, @"When allocating the last available block");
    

    /* Verify that allocating additional bytes triggers the allocation of new backing pages and a new free list entry */
    void *buffer_2;
    STAssertEquals(PLCRASH_ESUCCESS, allocator->alloc(&buffer_2, 64), @"Allocation failed");

    /* The free list should now have one entry */
    STAssertNotNULL(allocator->_free_list, @"The free list was not updated");
    STAssertTrue(allocator->_free_list == allocator->_free_list->_next, @"The free list has more than one entry, or in invalid _next value");

    allocator->dealloc(buffer);
    allocator->dealloc(buffer_2);
    
    delete allocator;
}

/* Test automatic page allocation when all remaining blocks are too small for the requested allocation */
- (void) testFragementationExhaustion {
    AsyncAllocator *allocator;
    STAssertEquals(PLCRASH_ESUCCESS, AsyncAllocator::Create(&allocator, 64), @"Failed to construct allocator");
    
    /* Save the initial free list state */
    AsyncAllocator::control_block *orig_free_list = allocator->_free_list;
    
    /* Allocate more than is available from the free list, triggering an additional page allocation.  */
    void *buffer;
    STAssertEquals(PLCRASH_ESUCCESS, allocator->alloc(&buffer, round_page(allocator->debug_bytes_free()) * 2), @"Allocation failed");

    /* The free list should now have two entries. */
    STAssertNotNULL(allocator->_free_list, @"The free list was not updated");
    STAssertEquals(allocator->_free_list, orig_free_list, "The free list pointer wasn't updated to point at the new free block's parent");
    STAssertNotEquals(allocator->_free_list, allocator->_free_list->_next, "No new free block was added");
    
    allocator->dealloc(buffer);
    delete allocator;
}

@end
