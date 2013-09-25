/*
 * Author: Mike Ash <mikeash@plausiblelabs.com>
 *
 * Copyright (c) 2012-2013 Plausible Labs Cooperative, Inc.
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

#import <dlfcn.h>
#import <mach-o/dyld.h>
#import <mach-o/getsect.h>

#include "PLCrashAsyncObjCSection.h"


@interface PLCrashAsyncObjCSectionTests : SenTestCase {
    /** The image containing our class. */
    plcrash_async_macho_t _image;
}

@end

@interface PLCrashAsyncObjCSectionTests (Category)

- (pl_vm_address_t) addressInCategory;

@end

/**
 * Simple class with one method, to make sure symbol lookups work for
 * a case with no categories or anything.
 */
@interface PLCrashAsyncObjCSectionTestsSimpleClass : NSObject

- (pl_vm_address_t) addressInSimpleClass;

@end

@implementation PLCrashAsyncObjCSectionTestsSimpleClass : NSObject

- (pl_vm_address_t) addressInSimpleClass {
    return [[[NSThread callStackReturnAddresses] objectAtIndex: 0] unsignedLongLongValue];
}

@end

@implementation PLCrashAsyncObjCSectionTests

+ (pl_vm_address_t) addressInClassMethod {
    return [[[NSThread callStackReturnAddresses] objectAtIndex: 0] unsignedLongLongValue];
}

- (void) setUp {
    /* Fetch our containing image's dyld info */
    Dl_info info;
    STAssertTrue(dladdr([self class], &info) > 0, @"Could not fetch dyld info for %p", [self class]);
    
    /* Look up the vmaddr slide for our image */
    pl_vm_off_t vmaddr_slide = 0;
    bool found_image = false;
    for (uint32_t i = 0; i < _dyld_image_count(); i++) {
        if (_dyld_get_image_header(i) == info.dli_fbase) {
            vmaddr_slide = _dyld_get_image_vmaddr_slide(i);
            found_image = true;
            break;
        }
    }
    STAssertTrue(found_image, @"Could not find dyld image record");
    
    plcrash_nasync_macho_init(&_image, mach_task_self(), info.dli_fname, (pl_vm_address_t) info.dli_fbase);
    
    /* Basic test of the initializer */
    STAssertEqualCStrings(_image.name, info.dli_fname, @"Incorrect name");
    STAssertEquals(_image.header_addr, (pl_vm_address_t) info.dli_fbase, @"Incorrect header address");
    STAssertEquals(_image.vmaddr_slide, (pl_vm_off_t) vmaddr_slide, @"Incorrect vmaddr_slide value");
    
    unsigned long text_size;
    STAssertNotNULL(getsegmentdata(info.dli_fbase, SEG_TEXT, &text_size), @"Failed to find segment");
    STAssertEquals(_image.text_size, (pl_vm_size_t) text_size, @"Incorrect text segment size computed");
}

- (void) tearDown {
    plcrash_nasync_macho_free(&_image);
}

static void ParseCallbackTrampoline(bool isClassMethod, plcrash_async_macho_string_t *className, plcrash_async_macho_string_t *methodName, pl_vm_address_t imp, void *ctx) {
    void (^block)(bool, plcrash_async_macho_string_t *, plcrash_async_macho_string_t *, pl_vm_address_t) = ctx;
    block(isClassMethod, className, methodName, imp);
}

/**
 * Verify our fixed ISA mask against the non-stable objc_debug_isa_class_mask.
 * Refer to plcrash_async_objc_supports_nonptr_isa and http://www.sealiesoftware.com/blog/archive/2013/09/24/objc_explain_Non-pointer_isa.html
 * for more details.
 */
- (void) testVerifyISAClassMask {
#ifdef __arm64__
    extern uint64_t objc_debug_isa_class_mask WEAK_IMPORT_ATTRIBUTE;

    /* Skip hosts where non-ptr isas are not supported */
    if (!plcrash_async_objc_supports_nonptr_isa(plcrash_async_macho_cpu_type(&_image)))
        return;

    STAssertNotNULL(objc_debug_isa_class_mask, @"The objc_debug_isa_class_mask variable is no longer vended");
    STAssertEquals(objc_debug_isa_class_mask, PLCRASH_ASYNC_OBJC_ISA_NONPTR_CLASS_MASK, @"Incorrect class mask");
#else
    STAssertFalse(plcrash_async_objc_supports_nonptr_isa(plcrash_async_macho_cpu_type(&_image)), @"This platform is declared to support non-ptr ISAs; the above tests should be enabled");
#endif
}

- (void) testParse {
    plcrash_error_t err;
    
    plcrash_async_objc_cache_t objCContext;
    err = plcrash_async_objc_cache_init(&objCContext);
    STAssertEquals(err, PLCRASH_ESUCCESS, @"pl_async_objc_context_init failed (that should not be possible, how did you do that?)");
    
    __block BOOL didCall = NO;
    uint64_t pc = [[[NSThread callStackReturnAddresses] objectAtIndex: 0] unsignedLongLongValue];
    err = plcrash_async_objc_find_method(&_image, &objCContext, pc, ParseCallbackTrampoline, ^(bool isClassMethod, plcrash_async_macho_string_t *className, plcrash_async_macho_string_t *methodName, pl_vm_address_t imp, void *ctx) {
        didCall = YES;
        plcrash_error_t err;
        
        pl_vm_size_t classNameLength;
        const char *classNamePtr;
        err = plcrash_async_macho_string_get_length(className, &classNameLength);
        STAssertEquals(err, PLCRASH_ESUCCESS, @"Failed to get length");
        err = plcrash_async_macho_string_get_pointer(className, &classNamePtr);
        STAssertEquals(err, PLCRASH_ESUCCESS, @"Failed to get pointer");
        
        pl_vm_size_t methodNameLength;
        const char *methodNamePtr;
        err = plcrash_async_macho_string_get_length(methodName, &methodNameLength);
        STAssertEquals(err, PLCRASH_ESUCCESS, @"Failed to get length");
        err = plcrash_async_macho_string_get_pointer(methodName, &methodNamePtr);
        STAssertEquals(err, PLCRASH_ESUCCESS, @"Failed to get pointer");
        
        NSString *classNameNS = [NSString stringWithFormat: @"%.*s", (int)classNameLength, classNamePtr];
        NSString *methodNameNS = [NSString stringWithFormat: @"%.*s", (int)methodNameLength, methodNamePtr];
        
        STAssertFalse(isClassMethod, @"Incorrectly indicated a class method");
        STAssertEqualObjects(classNameNS, NSStringFromClass([self class]), @"Class names don't match");
        STAssertEqualObjects(methodNameNS, NSStringFromSelector(_cmd), @"Method names don't match");
        STAssertEquals(imp, (pl_vm_address_t)[self methodForSelector: _cmd], @"Method IMPs don't match");
    });
    STAssertTrue(didCall, @"Method find callback never got called");
    STAssertEquals(err, PLCRASH_ESUCCESS, @"ObjC parse failed");
    
    didCall = NO;
    err = plcrash_async_objc_find_method(&_image, &objCContext, [self addressInCategory], ParseCallbackTrampoline, ^(bool isClassMethod, plcrash_async_macho_string_t *className, plcrash_async_macho_string_t *methodName, pl_vm_address_t imp, void *ctx) {
        didCall = YES;
        plcrash_error_t err;
        
        pl_vm_size_t classNameLength;
        const char *classNamePtr;
        err = plcrash_async_macho_string_get_length(className, &classNameLength);
        STAssertEquals(err, PLCRASH_ESUCCESS, @"Failed to get length");
        err = plcrash_async_macho_string_get_pointer(className, &classNamePtr);
        STAssertEquals(err, PLCRASH_ESUCCESS, @"Failed to get pointer");
        
        pl_vm_size_t methodNameLength;
        const char *methodNamePtr;
        err = plcrash_async_macho_string_get_length(methodName, &methodNameLength);
        STAssertEquals(err, PLCRASH_ESUCCESS, @"Failed to get length");
        err = plcrash_async_macho_string_get_pointer(methodName, &methodNamePtr);
        STAssertEquals(err, PLCRASH_ESUCCESS, @"Failed to get pointer");
        
        NSString *classNameNS = [NSString stringWithFormat: @"%.*s", (int)classNameLength, classNamePtr];
        NSString *methodNameNS = [NSString stringWithFormat: @"%.*s", (int)methodNameLength, methodNamePtr];
        
        STAssertFalse(isClassMethod, @"Incorrectly indicated a class method");
        STAssertEqualObjects(classNameNS, NSStringFromClass([self class]), @"Class names don't match");
        STAssertEqualObjects(methodNameNS, @"addressInCategory", @"Method names don't match");
        STAssertEquals(imp, (pl_vm_address_t)[self methodForSelector: @selector(addressInCategory)], @"Method IMPs don't match");
    });
    STAssertTrue(didCall, @"Method find callback never got called");
    STAssertEquals(err, PLCRASH_ESUCCESS, @"ObjC parse failed");
    
    PLCrashAsyncObjCSectionTestsSimpleClass *obj = [[[PLCrashAsyncObjCSectionTestsSimpleClass alloc] init] autorelease];
    didCall = NO;
    err = plcrash_async_objc_find_method(&_image, &objCContext, [obj addressInSimpleClass], ParseCallbackTrampoline, ^(bool isClassMethod, plcrash_async_macho_string_t *className, plcrash_async_macho_string_t *methodName, pl_vm_address_t imp, void *ctx) {
        didCall = YES;
        plcrash_error_t err;
        
        pl_vm_size_t classNameLength;
        const char *classNamePtr;
        err = plcrash_async_macho_string_get_length(className, &classNameLength);
        STAssertEquals(err, PLCRASH_ESUCCESS, @"Failed to get length");
        err = plcrash_async_macho_string_get_pointer(className, &classNamePtr);
        STAssertEquals(err, PLCRASH_ESUCCESS, @"Failed to get pointer");
        
        pl_vm_size_t methodNameLength;
        const char *methodNamePtr;
        err = plcrash_async_macho_string_get_length(methodName, &methodNameLength);
        STAssertEquals(err, PLCRASH_ESUCCESS, @"Failed to get length");
        err = plcrash_async_macho_string_get_pointer(methodName, &methodNamePtr);
        STAssertEquals(err, PLCRASH_ESUCCESS, @"Failed to get pointer");
        
        NSString *classNameNS = [NSString stringWithFormat: @"%.*s", (int)classNameLength, classNamePtr];
        NSString *methodNameNS = [NSString stringWithFormat: @"%.*s", (int)methodNameLength, methodNamePtr];
        
        STAssertFalse(isClassMethod, @"Incorrectly indicated a class method");
        STAssertEqualObjects(classNameNS, @"PLCrashAsyncObjCSectionTestsSimpleClass", @"Class names don't match");
        STAssertEqualObjects(methodNameNS, @"addressInSimpleClass", @"Method names don't match");
        STAssertEquals(imp, (pl_vm_address_t)[obj methodForSelector: @selector(addressInSimpleClass)], @"Method IMPs don't match");
    });
    STAssertTrue(didCall, @"Method find callback never got called");
    STAssertEquals(err, PLCRASH_ESUCCESS, @"ObjC parse failed");
    
    didCall = NO;
    err = plcrash_async_objc_find_method(&_image, &objCContext, [[self class] addressInClassMethod], ParseCallbackTrampoline, ^(bool isClassMethod, plcrash_async_macho_string_t *className, plcrash_async_macho_string_t *methodName, pl_vm_address_t imp, void *ctx) {
        didCall = YES;
        plcrash_error_t err;
        
        pl_vm_size_t classNameLength;
        const char *classNamePtr;
        err = plcrash_async_macho_string_get_length(className, &classNameLength);
        STAssertEquals(err, PLCRASH_ESUCCESS, @"Failed to get length");
        err = plcrash_async_macho_string_get_pointer(className, &classNamePtr);
        STAssertEquals(err, PLCRASH_ESUCCESS, @"Failed to get pointer");
        
        pl_vm_size_t methodNameLength;
        const char *methodNamePtr;
        err = plcrash_async_macho_string_get_length(methodName, &methodNameLength);
        STAssertEquals(err, PLCRASH_ESUCCESS, @"Failed to get length");
        err = plcrash_async_macho_string_get_pointer(methodName, &methodNamePtr);
        STAssertEquals(err, PLCRASH_ESUCCESS, @"Failed to get pointer");
        
        NSString *classNameNS = [NSString stringWithFormat: @"%.*s", (int)classNameLength, classNamePtr];
        NSString *methodNameNS = [NSString stringWithFormat: @"%.*s", (int)methodNameLength, methodNamePtr];
        
        STAssertTrue(isClassMethod, @"Incorrectly indicated an instance method");
        STAssertEqualObjects(classNameNS, NSStringFromClass([self class]), @"Class names don't match");
        STAssertEqualObjects(methodNameNS, @"addressInClassMethod", @"Method names don't match");
        STAssertEquals(imp, (pl_vm_address_t)[[self class] methodForSelector: @selector(addressInClassMethod)], @"Method IMPs don't match");
    });
    STAssertTrue(didCall, @"Method find callback never got called");
    STAssertEquals(err, PLCRASH_ESUCCESS, @"ObjC parse failed");
    
    plcrash_async_objc_cache_free(&objCContext);
}

@end

@implementation PLCrashAsyncObjCSectionTests (Category)

- (pl_vm_address_t) addressInCategory {
    return [[[NSThread callStackReturnAddresses] objectAtIndex: 0] unsignedLongLongValue];
}

@end
