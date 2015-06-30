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

#include "PLCrashMacros.h"
#include "PLCrashAsync.h"

/*
 * Provides a pure C interface to the C++ AsyncAllocator.
 *
 * This exists solely to support the remaining C/Objective-C code; once the codebase migrates to C/C++/Objective-C++,
 * we can drop the C compatibility support used here.
 */

#ifdef __cplusplus
#include "AsyncAllocator.hpp"
using plcrash_async_allocator_t = plcrash::async::AsyncAllocator;
#else
typedef struct plcrash_async_allocator plcrash_async_allocator_t;
#endif

PLCR_C_BEGIN_DECLS

PLCR_EXPORT plcrash_error_t plcrash_async_allocator_create (plcrash_async_allocator_t **allocator, size_t initial_size);

PLCR_EXPORT plcrash_error_t plcrash_async_allocator_alloc (plcrash_async_allocator_t *allocator, void **allocated, size_t size);
PLCR_EXPORT void plcrash_async_allocator_dealloc (plcrash_async_allocator_t *allocator, void *ptr);

PLCR_EXPORT void plcrash_async_allocator_free (plcrash_async_allocator_t *allocator);

PLCR_C_END_DECLS


#endif /* PLCRASH_ASYNC_ALLOCATOR_C_COMPAT_H */
