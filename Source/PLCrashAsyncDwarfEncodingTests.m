/*
 * Author: Landon Fuller <landonf@plausible.coop>
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

#import "PLCrashTestCase.h"

#include "PLCrashAsyncDwarfEncoding.h"

#if TARGET_OS_MAC && (!TARGET_OS_IPHONE)
#  define TEST_BINARY @"test.macosx"
#elif TARGET_IPHONE_SIMULATOR
#  define TEST_BINARY @"test.sim"
#elif TARGET_OS_IPHONE
#  define TEST_BINARY @"test.ios"
#else
#  error Unsupported target
#endif

@interface PLCrashAsyncDwarfEncodingTests : PLCrashTestCase {
    plcrash_async_macho_t _image;
    plcrash_async_mobject_t _eh_frame;
    plcrash_async_mobject_t _debug_frame;
}
@end

@implementation PLCrashAsyncDwarfEncodingTests

- (void) setUp {
    NSError *error;
    plcrash_error_t err;
    
    /* Map and load the binary */
    NSData *mappedImage = [self nativeBinaryFromTestResource: TEST_BINARY];
    STAssertNotNil(mappedImage, @"Failed to map image: %@", error);
    
    err = plcrash_nasync_macho_init(&_image, mach_task_self(), [TEST_BINARY UTF8String], [mappedImage bytes]);
    STAssertEquals(err, PLCRASH_ESUCCESS, @"Failed to initialize Mach-O parser");
    
    /* Map the eh/debug frame sections. We use our own fake __PL_DWARF segment to avoid toolchain interference with our test data. */
    err = plcrash_async_macho_map_section(&_image, "__PL_DWARF", "__eh_frame", &_eh_frame);
    STAssertEquals(err, PLCRASH_ESUCCESS, @"Failed to map __eh_frame section");
    
    err = plcrash_async_macho_map_section(&_image, "__PL_DWARF", "__debug_frame", &_debug_frame);
    STAssertEquals(err, PLCRASH_ESUCCESS, @"Failed to map __debug_frame section");
}

- (void) tearDown {
    plcrash_async_mobject_free(&_eh_frame);
    plcrash_async_mobject_free(&_debug_frame);
    plcrash_nasync_macho_free(&_image);
}

- (void) testFindFrameDescriptorEntry {
 // TODO
}

/**
 * Execute the Apple unwind regression tests.
 */
- (void) testRegression {
    // TODO;
    return;

    NSError *error;
    plcrash_error_t err;
    
    NSString *binPath = [self pathForTestResource: @"bins"];
    NSArray *cases = [[NSFileManager defaultManager] contentsOfDirectoryAtPath: binPath error: &error];
    STAssertNotNil(cases, @"Failed to read test case directory: %@", error);
    
    for (NSString *tcase in cases) {
        plcrash_async_macho_t image;
        
        /* Load and parse the image. */
        NSString *tcasePath = [binPath stringByAppendingPathComponent: tcase];
        NSData *mappedImage = [NSData dataWithContentsOfFile: tcasePath options: NSDataReadingMapped error: &error];
        STAssertNotNil(mappedImage, @"Failed to map image: %@", error);

        err = plcrash_nasync_macho_init(&image, mach_task_self(), [tcasePath UTF8String], [mappedImage bytes]);
        STAssertEquals(err, PLCRASH_ESUCCESS, @"Failed to initialize Mach-O parser");
        
        
        /* Map the (optional) eh/debug DWARF sections. */
        plcrash_async_mobject_t eh_frame;
        plcrash_async_mobject_t debug_frame;
        BOOL has_eh_frame = NO;
        BOOL has_debug_frame = NO;

        err = plcrash_async_macho_map_section(&image, "__DWARF", "__eh_frame", &eh_frame);
        if (err != PLCRASH_ENOTFOUND) {
            STAssertEquals(err, PLCRASH_ESUCCESS, @"Failed to map __eh_frame section for %@", tcase);
            has_eh_frame = YES;
        }

        err = plcrash_async_macho_map_section(&image, "__DWARF", "__debug_frame", &debug_frame);
        if (err != PLCRASH_ENOTFOUND) {
            STAssertEquals(err, PLCRASH_ESUCCESS, @"Failed to map __debug_frame section for %@", tcase);
            has_debug_frame = YES;
        }
        
        /* Smoke test the FDE parser */
        cpu_type_t cpu_type = plcrash_async_macho_cpu_type(&image);
        // TODO
        if (has_eh_frame) {
            plcrash_async_dwarf_frame_reader_t reader;
            plcrash_async_dwarf_frame_reader_init(&reader, &eh_frame, cpu_type);
        }

        if (has_debug_frame) {
            
        }
        
        /* Clean up */
        if (has_eh_frame)
            plcrash_async_mobject_free(&eh_frame);
        
        if (has_debug_frame)
            plcrash_async_mobject_free(&debug_frame);

        plcrash_nasync_macho_free(&image);
    }
}

@end
