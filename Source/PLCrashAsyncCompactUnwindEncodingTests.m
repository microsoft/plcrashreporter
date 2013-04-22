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
#import <mach-o/dyld.h>

#if TARGET_OS_MAC && (!TARGET_OS_IPHONE)
#define TEST_BINARY @"test.macosx"
#elif TARGET_OS_SIMULATOR
#define TEST_BINARY @"test.sim"
#elif TARGET_OS_IPHONE
#define TEST_BINARY @"test.ios"
#else
#error Unsupported target
#endif

/* The base PC value hard coded in our test CFE data */
#define BASE_PC 0

/* PC to use for the compact-common test */
#define PC_COMPACT_COMMON (BASE_PC+1)
#define PC_COMPACT_COMMON_ENCODING (UNWIND_X86_64_MODE_DWARF | PC_COMPACT_COMMON)

/* PC to use for the compact-private test */
#define PC_COMPACT_PRIVATE (BASE_PC+2)
#define PC_COMPACT_PRIVATE_ENCODING (UNWIND_X86_64_MODE_DWARF | PC_COMPACT_PRIVATE)

/* PC to use for the regular-common test */
#define PC_REGULAR (BASE_PC+10)
#define PC_REGULAR_ENCODING (UNWIND_X86_64_MODE_DWARF | PC_REGULAR)

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
    
    /** The mapped unwind data */
    plcrash_async_mobject_t _unwind_mobj;

    /** The CFE reader */
    plcrash_async_cfe_reader_t _reader;
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
    pl_vm_address_t header = plcrash_async_mobject_base_address(mobj);
    struct fat_header *fh = plcrash_async_mobject_remap_address(mobj, header, 0, sizeof(struct fat_header));
    STAssertNotNULL(fh, @"Could not load fat header");
    
    if (fh->magic != FAT_MAGIC && fh->magic != FAT_CIGAM)
        STFail(@"Not a fat binary!");
    
    /* Load all the fat architectures */
    struct fat_arch *base = plcrash_async_mobject_remap_address(mobj,
                                                                header,
                                                                sizeof(struct fat_header),
                                                                sizeof(struct fat_arch));
    uint32_t count = OSSwapBigToHostInt32(fh->nfat_arch);
    struct fat_arch *archs = calloc(count, sizeof(struct fat_arch));
    for (uint32_t i = 0; i < count; i++) {
        struct fat_arch *fa = &base[i];
        if (!plcrash_async_mobject_verify_local_pointer(mobj, fa, 0, sizeof(struct fat_arch))) {
            STFail(@"Pointer outside of mapped range");
        }
        
        archs[i].cputype = OSSwapBigToHostInt32(fa->cputype);
        archs[i].cpusubtype = OSSwapBigToHostInt32(fa->cpusubtype);
        archs[i].offset = OSSwapBigToHostInt32(fa->offset);
        archs[i].size = OSSwapBigToHostInt32(fa->size);
        archs[i].align = OSSwapBigToHostInt32(fa->align);
    }
    
    /* Find the right architecture; we based this on the first loaded Mach-O image, as NXGetLocalArchInfo returns
     * the incorrect i386 cpu type on x86-64. */
    const struct mach_header *hdr = _dyld_get_image_header(0);
    const struct fat_arch *best_arch = NXFindBestFatArch(hdr->cputype, hdr->cpusubtype, archs, count);

    /* Clean up */
    free(archs);
    
    /* Done! */
    *offset = best_arch->offset;
    *size = best_arch->size;
}

- (void) setUp {
    uint32_t offset, length;

    /*
     * Warning: This code assumes 1:1 correspondance between vmaddr/vmsize and foffset/fsize in the loaded binary.
     * This is currently the case with our test binaries, but it could possibly change in the future. To handle this,
     * one would either need to:
     * - Implement 'real' segment loading, ala https://github.com/landonf/libevil_patch/blob/b80ebf4c0442f234c4f3f9ec180a2f873c5e2559/libevil/libevil.m#L253
     * or
     * - Add a 'file mode' to the Mach-O parser that causes it to use file offsets rather than VM offsets.
     * or
     * - Don't bother to load all the segments properly, just map the CFE data.
     *
     * I didn't implement the file mode for the Mach-O parser as I'd like to keep that code as simple as possible,
     * given that it runs in a privileged crash time position, and 'file' mode is only required for unit tests.
     *
     * Performing segment loading or parsing the Mach-O binary isn't much work, so I'll probably just do that, and then
     * this comment can go away.
     */
    
    /* Load the image into a memory object */
    NSData *mappedImage = [self dataForTestResource: TEST_BINARY];
    plcrash_async_mobject_init(&_machoData, mach_task_self(), (pl_vm_address_t) [mappedImage bytes], [mappedImage length], true);
    /* Find a binary that matches the host */
    [self findBinary: &_machoData offset: &offset size: &length];
    void *macho_ptr = plcrash_async_mobject_remap_address(&_machoData, _machoData.task_address, offset, length);
    STAssertNotNULL(macho_ptr, @"Discovered binary is not within the mapped memory range");
    
    /* Parse the image */
    plcrash_error_t err;
    
    err = plcrash_nasync_macho_init(&_image, mach_task_self(), [TEST_BINARY UTF8String], macho_ptr);
    STAssertEquals(err, PLCRASH_ESUCCESS, @"Failed to initialize Mach-O parser");
    
    /* Map the unwind section */
    err = plcrash_async_macho_map_section(&_image, SEG_TEXT, "__unwind_info", &_unwind_mobj);
    STAssertEquals(err, PLCRASH_ESUCCESS, @"Failed to map unwind info");
    
    
    /* Initialize the CFE reader */
    cpu_type_t cputype = _image.byteorder->swap32(_image.header.cputype);
    err = plcrash_async_cfe_reader_init(&_reader, &_unwind_mobj, cputype);
    STAssertEquals(err, PLCRASH_ESUCCESS, @"Failed to initialize CFE reader");
    
}

- (void) tearDown {
    plcrash_nasync_macho_free(&_image);
    plcrash_async_mobject_free(&_machoData);
    plcrash_async_mobject_free(&_unwind_mobj);
    plcrash_async_cfe_reader_free(&_reader);
}

/**
 * Test reading of a PC, compressed, with a common encoding.
 */
- (void) testReadCompressedCommonEncoding {
    plcrash_error_t err;

    uint32_t encoding;
    err = plcrash_async_cfe_reader_find_pc(&_reader, PC_COMPACT_COMMON, &encoding);
    STAssertEquals(PLCRASH_ESUCCESS, err, @"Failed to locate CFE entry");
    STAssertEquals(encoding, (uint32_t)PC_COMPACT_COMMON_ENCODING, @"Incorrect encoding returned");
}

/**
 * Test reading of a PC, compressed, with a private encoding.
 */
- (void) testReadCompressedEncoding {
    plcrash_error_t err;
    
    uint32_t encoding;
    err = plcrash_async_cfe_reader_find_pc(&_reader, PC_COMPACT_PRIVATE, &encoding);
    STAssertEquals(PLCRASH_ESUCCESS, err, @"Failed to locate CFE entry");
    STAssertEquals(encoding, (uint32_t)PC_COMPACT_PRIVATE_ENCODING, @"Incorrect encoding returned");
}

/**
 * Test reading of a PC, regular, with a common encoding.
 */
- (void) testReadRegularEncoding {
    plcrash_error_t err;
    
    uint32_t encoding;
    err = plcrash_async_cfe_reader_find_pc(&_reader, PC_REGULAR, &encoding);
    STAssertEquals(PLCRASH_ESUCCESS, err, @"Failed to locate CFE entry");
    STAssertEquals(encoding, (uint32_t)PC_REGULAR_ENCODING, @"Incorrect encoding returned");
}

@end
