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

#import "SenTestCompat.h"


#include "DynamicLoader.hpp"

#include <dlfcn.h>
#include <mach-o/dyld.h>
#include <objc/runtime.h>

using namespace plcrash::async;

@interface PLCrashAsyncDynamicLoaderTests : SenTestCase

@end

@implementation PLCrashAsyncDynamicLoaderTests

- (void) testFindDynamicLoaderInfo {
    DynamicLoader *loader;
    AsyncAllocator *allocator;
    
    STAssertEquals(AsyncAllocator::Create(&allocator, 4 * 1024 * 1024 /* 4MB */), PLCRASH_ESUCCESS, @"Failed to create allocator");
    STAssertEquals(DynamicLoader::NonAsync_Create(&loader, allocator, mach_task_self()), PLCRASH_ESUCCESS, @"Failed to fetch dyld info");

    /* If this is an LP64 binary, so should be our dyld image data (and vis versa) */
#ifdef __LP64__
    integer_t expectedFormat = TASK_DYLD_ALL_IMAGE_INFO_64;
#else
    integer_t expectedFormat = TASK_DYLD_ALL_IMAGE_INFO_32;
#endif
    
    STAssertEquals(loader->dyld_info().all_image_info_format, expectedFormat, @"Got unexpected image info format");

    delete loader;
    delete allocator;
}

- (void) testFetchImageList {
    DynamicLoader *loader = nullptr;
    DynamicLoader::ImageList *images = nullptr;
    AsyncAllocator *allocator = nullptr;

    /* Create our allocator */
    STAssertEquals(AsyncAllocator::Create(&allocator, 64 * 1024 * 1024), PLCRASH_ESUCCESS, @"Failed to create allocator");
    
    /* Find our image info */
    STAssertEquals(DynamicLoader::NonAsync_Create(&loader, allocator, mach_task_self()), PLCRASH_ESUCCESS, @"Failed to fetch dyld info");

    /* Fetch our image list */
    STAssertEquals(loader->readImageList(allocator, &images), PLCRASH_ESUCCESS, @"Failed to read image list");
    STAssertNotNULL(images, @"Reading of images succeeded, but returned NULL!");
    
    /* Compare against our dyld-reported image list */
    STAssertEquals(images->count(), _dyld_image_count(), @"Our image count does not match dyld's!");
    
    size_t found = 0;
    for (size_t i = 0; i < images->count(); i++) {
        plcrash_async_macho_t *image = images->getImage(i);
        
        /* Search for a matching dyld image */
        for (uint32_t j = 0; j < _dyld_image_count(); j++) {
            
            /* Header must match */
            if ((pl_vm_address_t) _dyld_get_image_header(j) != image->header_addr)
                continue;
            
            /* If the header address matches, so should everything else. */
            STAssertEqualCStrings(_dyld_get_image_name(j), image->name, @"Name does not match!");
            STAssertEquals(_dyld_get_image_vmaddr_slide(j), image->vmaddr_slide, @"Slide does not match!");
            found++;
            break;
        }
    };
    STAssertEquals(found, images->count(), @"Could not find a match for all images!");
    
    
    /* Perform the same comparison in reverse, verifying that we didn't just find the *same* image N times
     * in the dyld list. */
    found = 0;
    for (uint32_t j = 0; j < _dyld_image_count(); j++) {
        for (size_t i = 0; i < images->count(); i++) {
            plcrash_async_macho_t *image = images->getImage(i);

            /* Header must match */
            if ((pl_vm_address_t) _dyld_get_image_header(j) != image->header_addr)
                continue;
            
            /* If the header address matches, so should everything else. */
            STAssertEqualCStrings(_dyld_get_image_name(j), image->name, @"Name does not match!");
            STAssertEquals(_dyld_get_image_vmaddr_slide(j), image->vmaddr_slide, @"Slide does not match!");
            found++;
            break;
        }
    }
    STAssertEquals(found, _dyld_image_count(), @"Could not find a match for all images!");

    delete images;
    delete loader;
    delete allocator;
}

- (void) testImageForAddress {
    DynamicLoader::ImageList *images = nullptr;
    AsyncAllocator *allocator = nullptr;
    
    /* Create our allocator */
    STAssertEquals(AsyncAllocator::Create(&allocator, 64 * 1024 * 1024), PLCRASH_ESUCCESS, @"Failed to create allocator");
    
    /* Fetch our image list */
    STAssertEquals(DynamicLoader::ImageList::NonAsync_Read(&images, allocator, mach_task_self()), PLCRASH_ESUCCESS, @"Failed to fetch dyld info");
    STAssertNotNULL(images, @"Reading of images succeeded, but returned NULL!");

    /* Fetch the our IMP address and symbolicate it using dladdr(). */
    IMP localIMP = class_getMethodImplementation([self class], _cmd);
    Dl_info dli;
    STAssertTrue(dladdr((void *)localIMP, &dli) != 0, @"Failed to look up symbol");
    
    /* Look up the vmaddr slide for our image */
    pl_vm_off_t vmaddr_slide = 0;
    bool found_image = false;
    for (uint32_t i = 0; i < _dyld_image_count(); i++) {
        if (_dyld_get_image_header(i) == dli.dli_fbase) {
            vmaddr_slide = _dyld_get_image_vmaddr_slide(i);
            found_image = true;
            break;
        }
    }
    STAssertTrue(found_image, @"Could not find dyld image record");
    
    /* Verify that image_base returns a valid value */
    plcrash_async_macho_t *image;
    plcrash_async_macho_t *current_image;
    
    image = current_image = images->imageContainingAddress((pl_vm_address_t) dli.dli_fbase);
    STAssertNotNULL(image, @"Failed to return valid image");
    STAssertEquals(image->header_addr, (pl_vm_address_t)dli.dli_fbase, @"Incorrect Mach-O header address");
    STAssertEquals(image->vmaddr_slide, vmaddr_slide, @"Incorrect slide value");
    
    /* Verify that image_base+image_length-1 returns our image. */
    STAssertEquals(current_image, images->imageContainingAddress((pl_vm_address_t) dli.dli_fbase + image->text_size-1), @"Failed to find our image for an address within base + text_size");
    
    /* Verify that image_base+image_length does not return our image. */
    STAssertNotEquals(current_image, images->imageContainingAddress((pl_vm_address_t) dli.dli_fbase + image->text_size), @"Returned our image for an address past the end of the text size");
    
    /* Verify that NULL is returned when an image is not found (we can be fairly certain that there's no image loaded at the NULL page!) */
    STAssertNULL(images->imageContainingAddress(0x0), @"Returned an image pointer for an impossible image address!");
    
    delete images;
    delete allocator;
}

@end
