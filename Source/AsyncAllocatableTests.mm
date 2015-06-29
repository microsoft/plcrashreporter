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

/* Enable debug access to the async allocator */
#define PLCF_ASYNCALLOCATOR_DEBUG 1
#import "AsyncAllocator.hpp"

#import "AsyncAllocatable.hpp"


PLCR_CPP_BEGIN_ASYNC_NS

/* An async-allocatable class to be used for testing */
class ExampleAsyncAllocatable : public AsyncAllocatable {
public:
    uint32_t _magic;
    ExampleAsyncAllocatable (uint32_t magic) : _magic(magic) {}
};

PLCR_CPP_END_ASYNC_NS

using namespace plcrash::async;

@interface PLCrashAsyncAllocatableTests : SenTestCase @end

@implementation PLCrashAsyncAllocatableTests

- (void) testRoundTripAllocation {
    AsyncAllocator *allocator;
    STAssertEquals(PLCRASH_ESUCCESS, AsyncAllocator::Create(&allocator, PAGE_SIZE), @"Failed to construct allocator");
    vm_size_t free_bytes = allocator->debug_bytes_free();
    
    /* Instantiate an instance via our allocator */
    ExampleAsyncAllocatable *eaa = new (allocator) ExampleAsyncAllocatable(0x4242);
    STAssertNotNULL(eaa, @"Allocation failed!");
    STAssertEquals(eaa->_magic, 0x4242, @"Instance was not correctly constructed");
    STAssertTrue(allocator->debug_bytes_free() < free_bytes, @"The bytes were not taken from the allocator free list");

    /* Verify that 'delete' correctly delegates to the AsyncAllocator */
    delete eaa;
    STAssertEquals(free_bytes, allocator->debug_bytes_free(), @"The bytes were not returned to the allocator free list");

    /* Clean up */
    delete allocator;
}

@end

