/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008-2011 Plausible Labs Cooperative, Inc.
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

#import "PLCrashAsyncImageList.h"
#import <mach-o/dyld.h>

@interface PLCrashAsyncImageTests : SenTestCase {
    plcrash_async_image_list_t _list;
}
@end

// XXX TODO: Decouple the async image list from the Mach-O parsing, such that
// we can properly test it independently of Mach-O.
//
// Some work on this has been done in the bitstadium-upstream branch, but
// no tests were written for the changes.
@implementation PLCrashAsyncImageTests

- (void) setUp {
    plcrash_async_image_list_init(&_list, mach_task_self());
}

- (void) tearDown {
    plcrash_async_image_list_free(&_list);
}

- (void) testAppendImage {
    // XXX - This is required due to the tight coupling with the Mach-O parser
    uint32_t count = _dyld_image_count();
    STAssertTrue(count >= 5, @"We need at least five Mach-O images for this test. This should not be a problem on a modern system.");
    
    plcrash_async_image_list_append(&_list, (pl_vm_address_t) _dyld_get_image_header(0), _dyld_get_image_vmaddr_slide(0), _dyld_get_image_name(0));

    STAssertNotNULL(_list.head, @"List HEAD should be set to our new image entry");
    STAssertEquals(_list.head, _list.tail, @"The list head and tail should be equal for the first entry");
    
    plcrash_async_image_list_append(&_list, (pl_vm_address_t) _dyld_get_image_header(1), _dyld_get_image_vmaddr_slide(1), _dyld_get_image_name(1));
    plcrash_async_image_list_append(&_list, (pl_vm_address_t) _dyld_get_image_header(2), _dyld_get_image_vmaddr_slide(2), _dyld_get_image_name(2));
    plcrash_async_image_list_append(&_list, (pl_vm_address_t) _dyld_get_image_header(3), _dyld_get_image_vmaddr_slide(3), _dyld_get_image_name(3));
    plcrash_async_image_list_append(&_list, (pl_vm_address_t) _dyld_get_image_header(4), _dyld_get_image_vmaddr_slide(4), _dyld_get_image_name(4));
    
    /* Verify the appended elements */
    plcrash_async_image_t *item = NULL;
    for (uintptr_t i = 0; i <= 5; i++) {
        /* Fetch the next item */
        item = plcrash_async_image_list_next(&_list, item);
        if (i <= 4) {
            STAssertNotNULL(item, @"Item should not be NULL");
        } else {
            STAssertNULL(item, @"Item should be NULL");
            break;
        }

        /* Validate its value */
        STAssertEquals((pl_vm_address_t) _dyld_get_image_header(i), item->macho_image.header_addr, @"Incorrect header value");
        STAssertEquals((int64_t)_dyld_get_image_vmaddr_slide(i), item->macho_image.vmaddr_slide, @"Incorrect slide value");
        STAssertEqualCStrings(_dyld_get_image_name(i), item->macho_image.name, @"Incorrect name value");
    }
}


/* Test removing the last image in the list. */
- (void) testRemoveLastImage {
    plcrash_async_image_list_append(&_list, 0x0, 0x10, "image_name");
    plcrash_async_image_list_remove(&_list, 0x0);

    STAssertNULL(_list.head, @"List HEAD should now be NULL");
    STAssertNULL(_list.tail, @"List TAIL should now be NULL");
}

- (void) testRemoveImage {
    // XXX - This is required due to the tight coupling with the Mach-O parser
    uint32_t count = _dyld_image_count();
    STAssertTrue(count >= 5, @"We need at least five Mach-O images for this test. This should not be a problem on a modern system.");

    plcrash_async_image_list_append(&_list, (pl_vm_address_t) _dyld_get_image_header(0), _dyld_get_image_vmaddr_slide(0), _dyld_get_image_name(0));
    plcrash_async_image_list_append(&_list, (pl_vm_address_t) _dyld_get_image_header(1), _dyld_get_image_vmaddr_slide(1), _dyld_get_image_name(1));
    plcrash_async_image_list_append(&_list, (pl_vm_address_t) _dyld_get_image_header(2), _dyld_get_image_vmaddr_slide(2), _dyld_get_image_name(2));
    plcrash_async_image_list_append(&_list, (pl_vm_address_t) _dyld_get_image_header(3), _dyld_get_image_vmaddr_slide(3), _dyld_get_image_name(3));
    plcrash_async_image_list_append(&_list, (pl_vm_address_t) _dyld_get_image_header(4), _dyld_get_image_vmaddr_slide(4), _dyld_get_image_name(4));

    /* Try a non-existent item */
    plcrash_async_image_list_remove(&_list, 0x42);

    /* Remove real items */
    plcrash_async_image_list_remove(&_list, (pl_vm_address_t) _dyld_get_image_header(1));
    plcrash_async_image_list_remove(&_list, (pl_vm_address_t) _dyld_get_image_header(3));

    /* Verify the contents of the list */
    plcrash_async_image_t *item = NULL;
    int val = 0x0;
    for (int i = 0; i <= 3; i++) {
        /* Fetch the next item */
        item = plcrash_async_image_list_next(&_list, item);
        if (i <= 2) {
            STAssertNotNULL(item, @"Item should not be NULL");
        } else {
            STAssertNULL(item, @"Item should be NULL");
            break;
        }
        
        /* Validate its value */
        STAssertEquals((pl_vm_address_t) _dyld_get_image_header(val), item->macho_image.header_addr, @"Incorrect header value for %d", val);
        STAssertEquals((int64_t)_dyld_get_image_vmaddr_slide(val), item->macho_image.vmaddr_slide, @"Incorrect slide value for %d", val);
        STAssertEqualCStrings(_dyld_get_image_name(val), item->macho_image.name, @"Incorrect name value for %d", val);
        val += 0x2;
    }
}

@end
