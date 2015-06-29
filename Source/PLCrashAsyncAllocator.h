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

#ifndef PLCRASH_ASYNC_ALLOCATOR_C_COMPAT_H
#define PLCRASH_ASYNC_ALLOCATOR_C_COMPAT_H

/*
 * Provides a pure C interface to the C++ AsyncAllocator.
 *
 * This exists solely to support the remaining C/Objective-C code; once the codebase migrates to C/C++/Objective-C++,
 * we can drop the C compatibility support used here.
 */

#ifdef __cplusplus
#include "AsyncAllocator.hpp"
#define PLCR_ASYNC_ALLOCATOR_T plcrash::async::AsyncAllocator*
#else
typedef struct AsyncAllocator *AsyncAllocator;
#define PLCR_ASYNC_ALLOCATOR_T AsyncAllocator
#endif

plcrash_error_t plcrash_async_allocator_create (PLCR_ASYNC_ALLOCATOR_T *allocator, size_t initial_size);

plcrash_error_t plcrash_async_allocator_alloc (PLCR_ASYNC_ALLOCATOR_T allocator, void **allocated, size_t size);
void plcrash_async_allocator_dealloc (PLCR_ASYNC_ALLOCATOR_T allocator, void *ptr);

void plcrash_async_allocator_free (PLCR_ASYNC_ALLOCATOR_T allocator);


#endif /* PLCRASH_ASYNC_ALLOCATOR_C_COMPAT_H */
