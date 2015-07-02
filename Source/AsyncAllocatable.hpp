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

#ifndef PLCRASH_ASYNC_ALLOCATABLE_H
#define PLCRASH_ASYNC_ALLOCATABLE_H


#include <unistd.h>
#include <stdint.h>

#include "PLCrashAsync.h"
#include "PLCrashMacros.h"

#include "AsyncAllocator.hpp"

/**
 * @internal
 * @ingroup plcrash_async
 *
 * @{
 */

PLCR_CPP_BEGIN_ASYNC_NS

/**
 * @internal
 *
 * Provides new and delete operators that may only be used in combination with
 * an async-safe allocator.
 */
class AsyncAllocatable {
public:
    void *operator new (size_t size, AsyncAllocator *allocator) throw();
    void *operator new[] (size_t size, AsyncAllocator *allocator) throw();
    
    void operator delete (void *ptr, size_t size);
    void operator delete[] (void *ptr, size_t size);
    
    virtual ~AsyncAllocatable () {};
    
private:
    /* Disallow non-async-safe allocation entirely. */
    void *operator new (size_t);
    void *operator new[] (size_t);
};

PLCR_CPP_END_ASYNC_NS

/**
 * @}
 */

#endif /* PLCRASH_ASYNC_ALLOCATABLE_H */
