/*
 * Author: Landon Fuller <landonf@plausible.coop>
 *
 * Copyright (c) 2015 Plausible Labs Cooperative, Inc.
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

#include "AsyncAllocatable.hpp"

/**
 * @internal
 * @ingroup plcrash_async
 *
 * @{
 */

PLCR_CPP_BEGIN_ASYNC_NS

/*
 * Shared new() implementation.
 */
static void *perform_new (size_t size, AsyncAllocator *allocator) {
    /* Try to allocate space for the instance. */
    void *buffer;
    plcrash_error_t err = allocator->alloc(&buffer, size);
    if (err != PLCRASH_ESUCCESS) {
        PLCF_DEBUG("async-safe new() allocation failed!");
        return NULL;
    }
    
    /* Return the buffer to be used for instance construction */
    return buffer;
}

/*
 * Shared delete() implementation.
 */
static void perform_delete (void *ptr, size_t size) {
    /* Find the allocator */
    AsyncAllocator *allocator = AsyncAllocator::allocator(ptr);
    
    /* Request deallocation */
    allocator->dealloc(ptr);
}

/**
 * Allocate a buffer of size @a via @a allocator. Returns NULL if allocation fails.
 *
 * @param size The size of the buffer to be allocated.
 * @param allocator The allocator from which the buffer will be allocated.
 */
void *AsyncAllocatable::operator new (size_t size, AsyncAllocator *allocator) throw() {
    return perform_new(size, allocator);
}

/**
 * Allocate a buffer of size @a via @a allocator. Returns NULL if allocation fails.
 *
 * @param size The size of the buffer to be allocated.
 * @param allocator The allocator from which the buffer will be allocated.
 */
void *AsyncAllocatable::operator new[] (size_t size, AsyncAllocator *allocator) throw() {
    return perform_new(size, allocator);
}

/**
 * Deallocate resources associated with @a ptr.
 *
 * @param ptr A buffer previously allocated with async-safe new.
 * @param size The size of the allocated buffer.
 */
void AsyncAllocatable::operator delete (void *ptr, size_t size) {
    return perform_delete(ptr, size);
}

/**
 * Deallocate resources associated with @a ptr.
 *
 * @param ptr A buffer previously allocated with async-safe new.
 * @param size The size of the allocated buffer.
 */
void AsyncAllocatable::operator delete[] (void *ptr, size_t size) {
    return perform_delete(ptr, size);
}

PLCR_CPP_END_ASYNC_NS

/**
 * @}
 */
