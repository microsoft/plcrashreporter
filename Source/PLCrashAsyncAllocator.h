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

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @internal
 * @ingroup plcrash_async
 *
 * Implements async-safe memory allocation
 *
 * @{
 */

#include <unistd.h>
#include <stdint.h>

#include "PLCrashAsync.h"

/**
 * Initialization options for plcrash_async_allocator_t.
 */
typedef enum {
    /**
     * Enable a low guard page. This will insert a PROT_NONE page prior to the
     * allocatable region, helping to ensure that a buffer overflow that occurs elsewhere
     * in the code will not overwrite the allocatable space.
     */
    PLCrashAsyncGuardLowPage = 1 << 0,
    
    /**
     * Enable a high guard page. This will insert a PROT_NONE page after to the
     * allocatable region, helping to ensure that a buffer overflow (including a stack overflow)
     * will be immediately detected. */
    PLCrashAsyncGuardHighPage = 1 << 1,
} PLCrashAsyncAllocatorOptions;

typedef struct plcrash_async_allocator plcrash_async_allocator_t;

plcrash_error_t plcrash_async_allocator_new (plcrash_async_allocator_t **allocator, size_t size, uint32_t options);
void *plcrash_async_allocator_alloc (plcrash_async_allocator_t *allocator, size_t size, bool no_assert);

/**
 * @}
 */
    
#ifdef __cplusplus
}
#endif

#endif /* PLCRASH_ASYNC_ALLOCATOR_H */
