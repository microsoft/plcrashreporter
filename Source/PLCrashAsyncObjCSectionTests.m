/*
 * Author: Mike Ash <mikeash@plausiblelabs.com>
 *
 * Copyright (c) 2012 Plausible Labs Cooperative, Inc.
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
    pl_async_macho_t _image;
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
    int64_t vmaddr_slide = 0;
    bool found_image = false;
    for (uint32_t i = 0; i < _dyld_image_count(); i++) {
        if (_dyld_get_image_header(i) == info.dli_fbase) {
            vmaddr_slide = _dyld_get_image_vmaddr_slide(i);
            found_image = true;
            break;
        }
    }
    STAssertTrue(found_image, @"Could not find dyld image record");
    
    pl_async_macho_init(&_image, mach_task_self(), info.dli_fname, (pl_vm_address_t) info.dli_fbase, vmaddr_slide);
    
    /* Basic test of the initializer */
    STAssertEqualCStrings(_image.name, info.dli_fname, @"Incorrect name");
    STAssertEquals(_image.header_addr, (pl_vm_address_t) info.dli_fbase, @"Incorrect header address");
    STAssertEquals(_image.vmaddr_slide, (int64_t) vmaddr_slide, @"Incorrect vmaddr_slide value");
    
    unsigned long text_size;
    STAssertNotNULL(getsegmentdata(info.dli_fbase, SEG_TEXT, &text_size), @"Failed to find segment");
    STAssertEquals(_image.text_size, (pl_vm_size_t) text_size, @"Incorrect text segment size computed");
}

- (void) tearDown {
    pl_async_macho_free(&_image);
}

static void ParseCallbackTrampoline(const char *className, pl_vm_size_t classNameLength, const char *methodName, pl_vm_size_t methodNameLength, pl_vm_address_t imp, void *ctx) {
    void (^block)(const char *, pl_vm_size_t, const char *, pl_vm_size_t, pl_vm_address_t) = ctx;
    block(className, classNameLength, methodName, methodNameLength, imp);
}

- (void) testParse {
    plcrash_error_t err;
    
    __block BOOL didCall = NO;
    uint64_t pc = [[[NSThread callStackReturnAddresses] objectAtIndex: 0] unsignedLongLongValue];
    err = pl_async_objc_find_method(&_image, pc, ParseCallbackTrampoline, ^(const char *className, pl_vm_size_t classNameLength, const char *methodName, pl_vm_size_t methodNameLength, pl_vm_address_t imp) {
        didCall = YES;
        NSString *classNameNS = [NSString stringWithFormat: @"%.*s", (int)classNameLength, className];
        NSString *methodNameNS = [NSString stringWithFormat: @"%.*s", (int)methodNameLength, methodName];
        
        STAssertEqualObjects(classNameNS, NSStringFromClass([self class]), @"Class names don't match");
        STAssertEqualObjects(methodNameNS, NSStringFromSelector(_cmd), @"Method names don't match");
        STAssertEquals(imp, (pl_vm_address_t)[self methodForSelector: _cmd], @"Method IMPs don't match");
    });
    STAssertTrue(didCall, @"Method find callback never got called");
    STAssertEquals(err, PLCRASH_ESUCCESS, @"ObjC parse failed");
    
    didCall = NO;
    err = pl_async_objc_find_method(&_image, [self addressInCategory], ParseCallbackTrampoline, ^(const char *className, pl_vm_size_t classNameLength, const char *methodName, pl_vm_size_t methodNameLength, pl_vm_address_t imp) {
        didCall = YES;
        NSString *classNameNS = [NSString stringWithFormat: @"%.*s", (int)classNameLength, className];
        NSString *methodNameNS = [NSString stringWithFormat: @"%.*s", (int)methodNameLength, methodName];
        
        STAssertEqualObjects(classNameNS, NSStringFromClass([self class]), @"Class names don't match");
        STAssertEqualObjects(methodNameNS, @"addressInCategory", @"Method names don't match");
        STAssertEquals(imp, (pl_vm_address_t)[self methodForSelector: @selector(addressInCategory)], @"Method IMPs don't match");
    });
    STAssertTrue(didCall, @"Method find callback never got called");
    STAssertEquals(err, PLCRASH_ESUCCESS, @"ObjC parse failed");
    
    PLCrashAsyncObjCSectionTestsSimpleClass *obj = [[[PLCrashAsyncObjCSectionTestsSimpleClass alloc] init] autorelease];
    didCall = NO;
    err = pl_async_objc_find_method(&_image, [obj addressInSimpleClass], ParseCallbackTrampoline, ^(const char *className, pl_vm_size_t classNameLength, const char *methodName, pl_vm_size_t methodNameLength, pl_vm_address_t imp) {
        didCall = YES;
        NSString *classNameNS = [NSString stringWithFormat: @"%.*s", (int)classNameLength, className];
        NSString *methodNameNS = [NSString stringWithFormat: @"%.*s", (int)methodNameLength, methodName];
        
        STAssertEqualObjects(classNameNS, @"PLCrashAsyncObjCSectionTestsSimpleClass", @"Class names don't match");
        STAssertEqualObjects(methodNameNS, @"addressInSimpleClass", @"Method names don't match");
        STAssertEquals(imp, (pl_vm_address_t)[obj methodForSelector: @selector(addressInSimpleClass)], @"Method IMPs don't match");
    });
    STAssertTrue(didCall, @"Method find callback never got called");
    STAssertEquals(err, PLCRASH_ESUCCESS, @"ObjC parse failed");
    
    didCall = NO;
    err = pl_async_objc_find_method(&_image, [[self class] addressInClassMethod], ParseCallbackTrampoline, ^(const char *className, pl_vm_size_t classNameLength, const char *methodName, pl_vm_size_t methodNameLength, pl_vm_address_t imp) {
        didCall = YES;
        NSString *classNameNS = [NSString stringWithFormat: @"%.*s", (int)classNameLength, className];
        NSString *methodNameNS = [NSString stringWithFormat: @"%.*s", (int)methodNameLength, methodName];
        
        STAssertEqualObjects(classNameNS, NSStringFromClass([self class]), @"Class names don't match");
        STAssertEqualObjects(methodNameNS, @"addressInClassMethod", @"Method names don't match");
        STAssertEquals(imp, (pl_vm_address_t)[[self class] methodForSelector: @selector(addressInClassMethod)], @"Method IMPs don't match");
    });
    STAssertTrue(didCall, @"Method find callback never got called");
    STAssertEquals(err, PLCRASH_ESUCCESS, @"ObjC parse failed");
}

@end

@implementation PLCrashAsyncObjCSectionTests (Category)

- (pl_vm_address_t) addressInCategory {
    return [[[NSThread callStackReturnAddresses] objectAtIndex: 0] unsignedLongLongValue];
}

@end
