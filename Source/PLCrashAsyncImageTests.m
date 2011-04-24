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

#import "PLCrashAsyncImage.h"

@interface PLCrashAsyncImageTests : SenTestCase {
    plcrash_async_image_list_t _list;
}
@end


@implementation PLCrashAsyncImageTests

- (void) setUp {
    plcrash_async_image_list_init(&_list);
}

- (void) tearDown {
    plcrash_async_image_list_free(&_list);
}

- (void) testAppendImage {
    plcrash_async_image_list_append(&_list, 0x0, "image_name");

    STAssertNotNULL(_list.head, @"List HEAD should be set to our new image entry");
    STAssertEquals(_list.head, _list.tail, @"The list head and tail should be equal for the first entry");
    
    plcrash_async_image_list_append(&_list, 0x1, "image_name");
    plcrash_async_image_list_append(&_list, 0x2, "image_name");
    plcrash_async_image_list_append(&_list, 0x3, "image_name");
    plcrash_async_image_list_append(&_list, 0x4, "image_name");
    
    /* Verify the appended elements */
    plcrash_async_image_t *item = NULL;
    for (intptr_t i = 0; i <= 5; i++) {
        /* Fetch the next item */
        item = plcrash_async_image_list_next(&_list, item);
        if (i <= 4) {
            STAssertNotNULL(item, @"Item should not be NULL");
        } else {
            STAssertNULL(item, @"Item should be NULL");
            break;
        }

        /* Validate its value */
        STAssertEquals(i, item->header, @"Incorrect header value");
        STAssertEqualCStrings("image_name", item->name, @"Incorrect name value");
    }
}


/* Test removing the last image in the list. */
- (void) testRemoveLastImage {
    plcrash_async_image_list_append(&_list, 0x0, "image_name");
    plcrash_async_image_list_remove(&_list, 0x0);

    STAssertNULL(_list.head, @"List HEAD should now be NULL");
    STAssertNULL(_list.tail, @"List TAIL should now be NULL");
}

- (void) testRemoveImage {
    plcrash_async_image_list_append(&_list, 0x0, "image_name");
    plcrash_async_image_list_append(&_list, 0x1, "image_name");
    plcrash_async_image_list_append(&_list, 0x2, "image_name");
    plcrash_async_image_list_append(&_list, 0x3, "image_name");
    plcrash_async_image_list_append(&_list, 0x4, "image_name");

    /* Try a non-existent item */
    plcrash_async_image_list_remove(&_list, 0x42);

    /* Remove real items */
    plcrash_async_image_list_remove(&_list, 0x1);
    plcrash_async_image_list_remove(&_list, 0x3);

    /* Verify the contents of the list */
    plcrash_async_image_t *item = NULL;
    intptr_t val = 0x0; 
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
        STAssertEquals(val, item->header, @"Incorrect header value");
        STAssertEqualCStrings("image_name", item->name, @"Incorrect name value");
        val += 0x2;
    }
}

@end
