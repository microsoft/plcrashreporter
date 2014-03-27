/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008-2014 Plausible Labs Cooperative, Inc.
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

#import "ImageAnnotationReader.hpp"

#import "PLCrashImageAnnotation.h"
#import "PLCrashAsyncMachOImage.h"

#import <dlfcn.h>
#import <mach-o/dyld.h>
#import <mach-o/getsect.h>
#import <objc/runtime.h>
#import <execinfo.h>

/** Image-wide annotation used in our unit tests below. Note that there can only be one such attribute set
 * in a given Mach-O image; our use of it here precludes use in other tests. */
PLCrashImageAnnotation ImageAnnotation PLCRASH_IMAGE_ANNOTATION_ATTRIBUTE;

@interface ImageAnnotationReaderTests : SenTestCase {
    /** The image containing our class. */
    plcrash_async_macho_t _image;
}
@end


@implementation ImageAnnotationReaderTests

- (void) setUp {
    /* Fetch our containing image's dyld info */
    Dl_info info;
    STAssertTrue(dladdr([self class], &info) > 0, @"Could not fetch dyld info for %p", [self class]);
    
    /* Look up the vmaddr and slide for our image */
    uintptr_t text_vmaddr;
    pl_vm_off_t vmaddr_slide = 0;
    bool found_image = false;
    for (uint32_t i = 0; i < _dyld_image_count(); i++) {
        if (_dyld_get_image_header(i) == info.dli_fbase) {
            vmaddr_slide = _dyld_get_image_vmaddr_slide(i);
            text_vmaddr = info.dli_fbase - vmaddr_slide;
            found_image = true;
            break;
        }
    }
    STAssertTrue(found_image, @"Could not find dyld image record");
    plcrash_nasync_macho_init(&_image, mach_task_self(), info.dli_fname, (pl_vm_address_t) info.dli_fbase);
    
    /* Zero our image annotation */
    ImageAnnotation.version = 0;
    ImageAnnotation.data = NULL;
    ImageAnnotation.data_size = 0;
}

- (void) tearDown {
    plcrash_nasync_macho_free(&_image);
}

/**
 * Test crash info lookup for images which provide annotation information via
 * the __DATA,__plcrash_info section.
 */
- (void) testFindAnnotation {
    plcrash_async_mobject_t annotation;
    
    /* Try to find the annotation; this should fail if the data_size is 0 */
    ImageAnnotation.data_size = 0;
    plcrash_error_t ret = plcrash_async_macho_find_annotation(&_image, &annotation);
    STAssertEquals(ret, PLCRASH_ENOTFOUND, @"Somehow found image annotation");
    
    
    /* Initialize the annotation and try finding it again */
    const char *test_data = "hello, world";
    ImageAnnotation.version = 0;
    ImageAnnotation.data = test_data;
    ImageAnnotation.data_size = strlen(test_data);
    
    ret = plcrash_async_macho_find_annotation(&_image, &annotation);
    STAssertEquals(ret, PLCRASH_ESUCCESS, @"Failed to get image annotation");
    STAssertEquals(annotation.length, (pl_vm_size_t)ImageAnnotation.data_size, @"Annotation length is wrong");
    
    
    /* Verify that the fetched annotation data is correct */
    void *data = plcrash_async_mobject_remap_address(&annotation, annotation.task_address, 0, annotation.length);
    STAssertNotNULL(data, @"Failed to remap annotation string");
    
    if (data) {
        /* If the last test failed, don't come here and crash. */
        STAssertEquals(strncmp((const void *) ImageAnnotation.data, data, annotation.length), 0, @"Annotation data is wrong");
    }
    
    
    /* Clean up */
    plcrash_async_mobject_free(&annotation);
}

/**
 * Test crash info lookup to make sure it handles invalid data correctly.
 */
- (void) testFindAnnotationInvalid {
    plcrash_async_mobject_t annotation;
    
    /* Search on a NULL Mach-O image should return EINVAL */
    plcrash_error_t ret = plcrash_async_macho_find_annotation(NULL, &annotation);
    STAssertEquals(ret, PLCRASH_EINVAL, @"Didn't return invalid status");
    
    
    /* Search with a NULL annotation argument should return EINVAL */
    ret = plcrash_async_macho_find_annotation(&_image, NULL);
    STAssertEquals(ret, PLCRASH_EINVAL, @"Didn't return invalid status");
    
    
    /* Search with all NULL arguments should return EINVAL */
    ret = plcrash_async_macho_find_annotation(NULL, NULL);
    STAssertEquals(ret, PLCRASH_EINVAL, @"Didn't return invalid status");
    
    
    /* Search on an annotation with a NULL data pointer should return PLCRASH_EINVALID_DATA */
    ImageAnnotation.version = 0;
    ImageAnnotation.data = NULL;
    ImageAnnotation.data_size = 42;
    ret = plcrash_async_macho_find_annotation(&_image, &annotation);
    STAssertEquals(ret, PLCRASH_EINVALID_DATA, @"Didn't return invalid status");
    
    
    /* Search on an annotation with an unsupported version (1) should return PLCRASH_EINVALID_DATA */
    ImageAnnotation.version = 1;
    ImageAnnotation.data = "123";
    ImageAnnotation.data_size = 3;
    ret = plcrash_async_macho_find_annotation(&_image, &annotation);
    STAssertEquals(ret, PLCRASH_EINVALID_DATA, @"Didn't check version");
}


@end
