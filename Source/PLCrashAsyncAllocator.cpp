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

#include "PLCrashAsyncAllocator.h"

using namespace plcrash::async;

/**
 * Equivalent to AsyncAllocator::Create();
 */
plcrash_error_t plcrash_async_allocator_create (plcrash_async_allocator_t **allocator, size_t initial_size) {
    return AsyncAllocator::Create(allocator, initial_size);
}

/**
 * Equivalent to AsyncAllocator::alloc();
 */
plcrash_error_t plcrash_async_allocator_alloc (plcrash_async_allocator_t *allocator, void **allocated, size_t size) {
    return allocator->alloc(allocated, size);
}

/**
 * Equivalent to AsyncAllocator::dealloc();
 */
void plcrash_async_allocator_dealloc (plcrash_async_allocator_t *allocator, void *ptr) {
    return allocator->dealloc(ptr);
}

/**
 * Equivalent to `delete AsyncAllocator`;
 */
void plcrash_async_allocator_free (plcrash_async_allocator_t *allocator) {
    delete allocator;
}
