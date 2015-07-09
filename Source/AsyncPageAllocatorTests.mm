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
#import <mach/vm_region.h>

using namespace plcrash::async;

@interface PLCrashAsyncPageAllocatorTests : SenTestCase {
@private
}

@end

@implementation PLCrashAsyncPageAllocatorTests

/**
 * Test allocation of guard pages.
 */
- (void) testGuardPages {
    AsyncPageAllocator *allocator;
    plcrash_error_t err;
    kern_return_t kr;
    
    /* Create our test allocator */
    err = AsyncPageAllocator::Create(&allocator, PAGE_SIZE, AsyncPageAllocator::GuardLowPage | AsyncPageAllocator::GuardHighPage);
    STAssertEquals(PLCRASH_ESUCCESS, err, @"Failed to construct allocator");
    
    /* Calculate the start and end addresses */
    vm_address_t valid_start = trunc_page(allocator->usable_address());
    vm_address_t valid_end = allocator->usable_address() + allocator->usable_size();
    STAssertEquals(valid_end, valid_start + PAGE_SIZE, @"The usable address range is not within the single PAGE_SIZE allocation requested");

    /* Verify that the guard pages exist and have the expected settings. */
    vm_address_t guard_addr;
    vm_size_t guard_size;
    vm_region_basic_info_data_64_t guard_vm_info;
    mach_msg_type_number_t guard_vm_info_len;
    mach_port_t vm_obj;
    
    /* Validate the leading guard page */
    vm_address_t low_guard_addr = valid_start - PAGE_SIZE;
    
    guard_addr = low_guard_addr;
    guard_size = PAGE_SIZE;
    guard_vm_info_len = VM_REGION_BASIC_INFO_COUNT_64;
    
    kr = vm_region_64(mach_task_self(), &guard_addr, &guard_size, VM_REGION_BASIC_INFO_64, (vm_region_info_t) &guard_vm_info, &guard_vm_info_len, &vm_obj);
    STAssertEquals(KERN_SUCCESS, err, @"Failed to fetch VM info");
    STAssertEquals(guard_addr, low_guard_addr, @"Incorrect base address");
    STAssertEquals(guard_size, PAGE_SIZE, @"Guard region is not equal to PAGE_SIZE");
    
    STAssertTrue((guard_vm_info.protection & VM_PROT_WRITE) == 0, @"Page should not be writable!");
    STAssertTrue((guard_vm_info.protection & VM_PROT_READ) == 0, @"Page should not be readable!");
    
    
    /* Validate the trailing guard page */
    vm_address_t high_guard_addr = valid_end;
    
    guard_addr = high_guard_addr;
    guard_size = PAGE_SIZE;
    guard_vm_info_len = VM_REGION_BASIC_INFO_COUNT_64;
    
    kr = vm_region_64(mach_task_self(), &guard_addr, &guard_size, VM_REGION_BASIC_INFO_64, (vm_region_info_t) &guard_vm_info, &guard_vm_info_len, &vm_obj);
    STAssertEquals(KERN_SUCCESS, err, @"Failed to fetch VM info");
    STAssertEquals(guard_addr, high_guard_addr, @"Incorrect base address");
    STAssertEquals(guard_size, PAGE_SIZE, @"Guard region is not equal to PAGE_SIZE");
    
    STAssertTrue((guard_vm_info.protection & VM_PROT_WRITE) == 0, @"Page should not be writable!");
    STAssertTrue((guard_vm_info.protection & VM_PROT_READ) == 0, @"Page should not be readable!");
    
    /* Clean up */
    delete allocator;
}

@end
