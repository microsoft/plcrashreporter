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

#import "PLCrashAsyncCompactUnwindEncoding.h"
#import "PLCrashAsyncMachOImage.h"

#import <TargetConditionals.h>

#import <mach-o/fat.h>
#import <mach-o/arch.h>

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
@interface PLCrashAsyncCompactUnwindEncodingTests : SenTestCase {
@private
    /** A mapped Mach-O file */
    plcrash_async_mobject_t _machoData;
    
    /** The parsed Mach-O file (this will be a subset of _imageData) */
    plcrash_async_macho_t _image;
}
@end

@implementation PLCrashAsyncCompactUnwindEncodingTests

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
    NSData *result = [NSData dataWithContentsOfFile: path options: NSDataReadingUncached error: &error];
    NSAssert(result != nil, @"Failed to load resource data: %@", error);
    
    return result;
}

/**
 * Search the Mach-O FAT binary mapped by @a mobj for a fat architecture that best matches the host architecture,
 * and then return the architecture's image @a size and @a offset from the head of @a mobj;
 */
- (void) findBinary: (plcrash_async_mobject_t *) mobj offset: (uint32_t *) offset size: (uint32_t *) size {
    struct fat_header *fh = plcrash_async_mobject_remap_address(mobj, mobj->task_address, 0, sizeof(struct fat_header));
    STAssertNotNULL(fh, @"Could not load fat header");
    
    if (fh->magic != FAT_MAGIC && fh->magic != FAT_CIGAM)
        STFail(@"Not a fat binary!");
    
    /* Load all the fat architectures */
    pl_vm_address_t header = plcrash_async_mobject_base_address(mobj);
    struct fat_arch *base = plcrash_async_mobject_remap_address(mobj, header, sizeof(*fh), sizeof(*fh));
    uint32_t count = OSSwapBigToHostInt32(fh->nfat_arch);
    struct fat_arch *archs = calloc(count, sizeof(*archs));
    for (uint32_t i = 0; i < count; i++) {
        struct fat_arch *fa = &base[i];
        if (!plcrash_async_mobject_verify_local_pointer(mobj, fa, 0, sizeof(*fa))) {
            STFail(@"Pointer outside of mapped range");
        }
        
        archs[i].cputype = OSSwapBigToHostInt32(fa->cputype);
        archs[i].cpusubtype = OSSwapBigToHostInt32(fa->cpusubtype);
        archs[i].offset = OSSwapBigToHostInt32(fa->offset);
        archs[i].size = OSSwapBigToHostInt32(fa->size);
        archs[i].align = OSSwapBigToHostInt32(fa->align);
    }
    
    /* Find the right architecture */
    const NXArchInfo *local_arch = NXGetLocalArchInfo();
    const struct fat_arch *best_arch = NXFindBestFatArch(local_arch->cputype, local_arch->cpusubtype, archs, count);
    STAssertNotNULL(best_arch, @"Could not find a matching entry for the host architecture");
    
    /* Clean up */
    free(archs);
    
    /* Done! */
    *offset = best_arch->offset;
    *size = best_arch->size;
}

- (void) setUp {
    uint32_t offset, length;
    
    /* Load the image into a memory object */
    NSData *mappedImage = [self dataForTestResource: TEST_BINARY];
    plcrash_async_mobject_init(&_machoData, mach_task_self(), (pl_vm_address_t) [mappedImage bytes], [mappedImage length]);
    /* Find a binary that matches the host */
    [self findBinary: &_machoData offset: &offset size: &length];
    void *macho_ptr = plcrash_async_mobject_remap_address(&_machoData, _machoData.task_address, offset, length);
    STAssertNotNULL(macho_ptr, @"Discovered binary is not within the mapped memory range");
    
    /* Parse the image */
    plcrash_error_t err;
    
    err = plcrash_nasync_macho_init(&_image, mach_task_self(), [TEST_BINARY UTF8String], macho_ptr);
    STAssertEquals(err, PLCRASH_ESUCCESS, @"Failed to initialize Mach-O parser");
}

- (void) tearDown {
    plcrash_nasync_macho_free(&_image);
    plcrash_async_mobject_free(&_machoData);
}

/**
 * Test reader initialization.
 */
- (void) testInitReader {
    plcrash_async_cfe_reader_t reader;
    plcrash_async_mobject_t mobj;
    plcrash_error_t err;

    err = plcrash_async_macho_map_section(&_image, SEG_TEXT, "__unwind_info", &mobj);
    STAssertEquals(err, PLCRASH_ESUCCESS, @"Failed to map unwind info");
    
    cpu_type_t cputype = _image.byteorder->swap32(_image.header.cputype);
    err = plcrash_async_cfe_reader_init(&reader, &mobj, cputype);
    STAssertEquals(err, PLCRASH_ESUCCESS, @"Failed to initialize CFE reader");

    pl_vm_address_t mainPC;
    err = plcrash_async_macho_find_symbol_by_name(&_image, "_main", &mainPC);
    STAssertEquals(PLCRASH_ESUCCESS, err, @"Failed to locate main symbol");

    err = plcrash_async_cfe_reader_find_ip(&reader, mainPC);
    STAssertEquals(PLCRASH_ESUCCESS, err, @"Failed to locate CFE entry for main");

    plcrash_async_cfe_reader_free(&reader);
}

@end
