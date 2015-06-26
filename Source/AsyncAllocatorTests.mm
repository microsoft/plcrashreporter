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
#import "AsyncAllocator.hpp"
#import <mach/vm_region.h>

using namespace plcrash::async;

@interface PLCrashAsyncAllocatorTests : SenTestCase {
@private
}

@end

@implementation PLCrashAsyncAllocatorTests

/**
 * TODO: Properly exercise the allocator code paths.
 */
- (void) testTODO {
    AsyncAllocator *allocator;
    plcrash_error_t err;
    
    /* Create our test allocator */
    err = AsyncAllocator::Create(&allocator, PAGE_SIZE);
    STAssertEquals(PLCRASH_ESUCCESS, err, @"Failed to construct allocator");

    
    /* Get a reference into the useable page allocation by allocating a few bytes. */
    void *buffer;
    err = allocator->alloc(&buffer, 4);
    STAssertEquals(PLCRASH_ESUCCESS, err, @"Failed to allocate a full page");
    STAssertNotNULL(buffer, @"Failed to allocate page");
    allocator->dealloc(buffer);
    
    /* Clean up */
    delete allocator;
}

@end
