/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008-2012 Plausible Labs Cooperative, Inc.
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

#import "GTMSenTestCase.h"
#import "PLCrashAsyncMObject.h"

@interface PLCrashAsyncMObjectTests : SenTestCase {
@private
}

@end


@implementation PLCrashAsyncMObjectTests

- (void) test_mapMobj {
    size_t size = vm_page_size+1;
    uint8_t template[size];
    
    /* Create a map target */
    memset_pattern4(template, (const uint8_t[]){ 0xC, 0xA, 0xF, 0xE }, size);
    
    /* Map the memory */
    plcrash_async_mobject_t mobj;
    STAssertEquals(PLCRASH_ESUCCESS, plcrash_async_mobject_init(&mobj, mach_task_self(), (pl_vm_address_t)template, size), @"Failed to initialize mapping");
    
    /* Verify the mapped data */
    STAssertTrue(memcmp((void *)mobj.address, template, size) == 0, @"Mapping appears to be incorrect");
    
    /* Verify the vm_slide */
    STAssertEquals((pl_vm_address_t)template, (pl_vm_address_t) (mobj.address + mobj.vm_slide), @"Incorrect slide value!");
    
    /* Sanity check the length */
    STAssertEquals(mobj.length, (pl_vm_size_t)size, @"Incorrect length");
    
    /* Clean up */
    plcrash_async_mobject_free(&mobj);
}


/**
 * Test mapped object pointer validation.
 */
- (void) test_mapMobj_map_address {
    size_t size = vm_page_size+1;
    uint8_t template[size];
    
    /* Map the memory */
    plcrash_async_mobject_t mobj;
    STAssertEquals(PLCRASH_ESUCCESS, plcrash_async_mobject_init(&mobj, mach_task_self(), (pl_vm_address_t)template, size), @"Failed to initialize mapping");
    STAssertEquals((pl_vm_address_t)template, (pl_vm_address_t) (mobj.address + mobj.vm_slide), @"Incorrect slide value!");
    
    /* Test slide handling */
    STAssertEquals(mobj.address+1, plcrash_async_mobject_remap_address(&mobj, (pl_vm_address_t)template+1), @"Mapped to incorrect address");
    
    /* Clean up */
    plcrash_async_mobject_free(&mobj);
}

/**
 * Test mapped object pointer validation.
 */
- (void) test_mapMobj_pointer {
    size_t size = vm_page_size+1;
    uint8_t template[size];
    
    /* Map the memory */
    plcrash_async_mobject_t mobj;
    STAssertEquals(PLCRASH_ESUCCESS, plcrash_async_mobject_init(&mobj, mach_task_self(), (pl_vm_address_t)template, size), @"Failed to initialize mapping");
    
    /* Test the address range validation */
    STAssertNULL(plcrash_async_mobject_pointer(&mobj, mobj.address-1, 10), @"Returned pointer for a range that starts before our memory object");
    STAssertNULL(plcrash_async_mobject_pointer(&mobj, mobj.address+mobj.length-1, 10), @"Returned pointer for a range that ends after our memory object");
    STAssertNULL(plcrash_async_mobject_pointer(&mobj, mobj.address-10, 5), @"Returned pointer for a range that is entirely outside our memory object");
    STAssertNULL(plcrash_async_mobject_pointer(&mobj, mobj.address+mobj.length, 1), @"Returned pointer for a range that starts the end of our memory object");
    STAssertNULL(plcrash_async_mobject_pointer(&mobj, mobj.address, size+1), @"Returned pointer for a range that ends just past our memory object");
    
    STAssertNotNULL(plcrash_async_mobject_pointer(&mobj, mobj.address, size), @"Returned NULL for a range that comprises our entire memory object");
    STAssertNotNULL(plcrash_async_mobject_pointer(&mobj, mobj.address, size-1), @"Returned NULL for a range entirely within our memory object");
    
    /* Clean up */
    plcrash_async_mobject_free(&mobj);
}

@end