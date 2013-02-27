/*
 * Copyright (c) 2013 Plausible Labs Cooperative, Inc.
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
#import "PLCrashFrameCompactUnwind.h"
#import "PLCrashAsyncMachOImage.h"

#import <TargetConditionals.h>

#if TARGET_OS_MAC && (!TARGET_OS_IPHONE)
#define TEST_BINARY @"test.macosx"
#elif TARGET_OS_SIMULATOR
#define TEST_BINARY @"test.sim"
#elif TARGET_OS_IPHONE
#define TEST_BINARY @"test.ios"
#else
#error Unsupported target
#endif

/**
 * @internal
 *
 * This code tests compact frame unwinding.
 */
@interface PLCrashFrameCompactUnwindTests : SenTestCase @end

@implementation PLCrashFrameCompactUnwindTests

/**
 * Return the full path to the request test resource.
 *
 * @param resourceName Relative resource path.
 *
 * Test resources are located in Bundle/Resources/Tests/TestClass/ResourceName
 */
- (NSString *) pathForTestResource: (NSString *) resourceName {
    NSString *className = NSStringFromClass([self class]);
    NSString *bundleResources = [[NSBundle bundleForClass: [self class]] resourcePath];
    NSString *testResources = [bundleResources stringByAppendingPathComponent: @"Tests"];
    NSString *testRoot = [testResources stringByAppendingPathComponent: className];

    return [testRoot stringByAppendingPathComponent: resourceName];
}

/**
 * Find the test resource with the given @a resourceName, and load the resource's data.
 *
 * @param resourceName Relative resource path.
 */
- (NSData *) dataForTestResource: (NSString *) resourceName {
    NSError *error;
    NSString *path = [self pathForTestResource: resourceName];
    NSData *result = [NSData dataWithContentsOfFile: path options: NSDataReadingMappedIfSafe error: &error];
    NSAssert(result != nil, @"Failed to load resource data: %@", error);
    
    return result;
}

- (void) testSomething {
#if 0
    /* Load the image into a memory object */
    NSData *mappedImage = [self dataForTestResource: TEST_BINARY];
    plcrash_async_mobject_t imageData;
    plcrash_async_mobject_init(&imageData, mach_task_self(), (pl_vm_address_t) [mappedImage bytes], [mappedImage length]);

    /* Parse the image */
    plcrash_async_macho_t image;
    plcrash_async_mobject_t sect;
    plcrash_error_t err;

    err = plcrash_nasync_macho_init(&image, mach_task_self(), [TEST_BINARY UTF8String], imageData.address, 0);
    STAssertEquals(err, PLCRASH_ESUCCESS, @"Failed to initialize Mach-O parser");

    err = plcrash_async_macho_map_section(&image, SEG_TEXT, "__unwind_info", &sect);
    STAssertEquals(err, PLCRASH_ESUCCESS, @"Failed to map unwind info");

    plcrash_async_mobject_free(&sect);
#endif
}

@end
