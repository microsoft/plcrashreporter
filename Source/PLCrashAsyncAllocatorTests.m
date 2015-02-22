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
#import "PLCrashAsyncAllocator.h"

@interface PLCrashAsyncAllocatorTests : SenTestCase {
@private
}

@end

@implementation PLCrashAsyncAllocatorTests

/**
 * Test allocation of guard pages.
 */
- (void) testGuardPages {
    plcrash_async_allocator_t *alloc;
    plcrash_error_t err;

    err = plcrash_async_allocator_new(&alloc, PAGE_SIZE, PLCrashAsyncGuardLowPage|PLCrashAsyncGuardHighPage);
    STAssertEquals(PLCRASH_ESUCCESS, err, @"Failed to initialize allocator");

    void *buffer = plcrash_async_allocator_alloc(alloc, PAGE_SIZE, true);
    STAssertNotNULL(buffer, @"Failed to allocate page");
    // TODO
    // XXX missing free();
}

@end
