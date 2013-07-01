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

#include "dwarf_encoding_test.h"

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
    /* Loaded test image */
    plcrash_async_macho_t _image;

    /* Mapped __(PL)_DWARF sections */
    plcrash_async_mobject_t _eh_frame;
    plcrash_async_mobject_t _debug_frame;

    /* Frame readers */
    plcrash_async_dwarf_frame_reader_t _eh_reader;
    plcrash_async_dwarf_frame_reader_t _debug_reader;
}
@end

@implementation PLCrashAsyncDwarfEncodingTests

- (void) setUp {
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
    
    /* Determine the address size */
    const plcrash_async_byteorder_t *byteorder = plcrash_async_macho_byteorder(&_image);
    cpu_type_t cputype = byteorder->swap32(_image.header.cputype);
    uint8_t address_size = 4;
    if (cputype & CPU_ARCH_ABI64)
        address_size = 8;

    /* Initialize eh/debug readers */
    err = plcrash_async_dwarf_frame_reader_init(&_eh_reader, &_eh_frame, byteorder, address_size, false);
    STAssertEquals(PLCRASH_ESUCCESS, err, @"Failed to initialize reader");

    err = plcrash_async_dwarf_frame_reader_init(&_debug_reader, &_debug_frame, byteorder, address_size, true);
    STAssertEquals(PLCRASH_ESUCCESS, err, @"Failed to initialize reader");
}

- (void) tearDown {
    plcrash_async_dwarf_frame_reader_free(&_eh_reader);
    plcrash_async_dwarf_frame_reader_free(&_debug_reader);

    plcrash_async_mobject_free(&_eh_frame);
    plcrash_async_mobject_free(&_debug_frame);

    plcrash_nasync_macho_free(&_image);
}

- (void) testFindEHFrameDescriptorEntry {
    plcrash_error_t err;
    plcrash_async_dwarf_fde_info_t fde_info;

    err = plcrash_async_dwarf_frame_reader_find_fde(&_eh_reader, 0x0, PL_CFI_EH_FRAME_PC+PL_CFI_EH_FRAME_PC_RANGE-1, &fde_info);
    STAssertEquals(PLCRASH_ESUCCESS, err, @"FDE search failed");
    
    /* Should be the second entry in the table, plus the initial length field. */
    if (_eh_reader.address_size == 8) {
        STAssertEquals(fde_info.fde_offset, (pl_vm_address_t) (sizeof(pl_cfi_entry)) + PL_CFI_LEN_SIZE_64, @"Incorrect offset");
        STAssertEquals(fde_info.fde_length, (uint64_t)PL_CFI_SIZE_64, @"Incorrect length");
    } else {
        STAssertEquals(fde_info.fde_offset, (pl_vm_address_t) (sizeof(pl_cfi_entry)) + PL_CFI_LEN_SIZE_32, @"Incorrect offset");
        STAssertEquals(fde_info.fde_length, (uint64_t)PL_CFI_SIZE_32, @"Incorrect length");
    }
    //STAssertEquals(fde_info.fde_instruction_offset, (pl_vm_address_t)0x0, @"Incorrect instruction offset (should be the first entry)");

    plcrash_async_dwarf_fde_info_free(&fde_info);

    /* Verify that an unknown PC returns ENOTFOUND. */
    err = plcrash_async_dwarf_frame_reader_find_fde(&_debug_reader, 0x0, PL_CFI_EH_FRAME_PC+PL_CFI_EH_FRAME_PC_RANGE, &fde_info);
    STAssertEquals(PLCRASH_ENOTFOUND, err, @"FDE should not have been found");
}

- (void) testFindDebugFrameDescriptorEntry {
    plcrash_error_t err;
    plcrash_async_dwarf_fde_info_t fde_info;

    err = plcrash_async_dwarf_frame_reader_find_fde(&_debug_reader, 0x0, PL_CFI_DEBUG_FRAME_PC+PL_CFI_DEBUG_FRAME_PC_RANGE-1, &fde_info);
    STAssertEquals(PLCRASH_ESUCCESS, err, @"FDE search failed");
    
    /* Should be the second entry in the table, plus the initial length field. */
    if (_eh_reader.address_size == 8) {
        STAssertEquals(fde_info.fde_offset, (pl_vm_address_t) (sizeof(pl_cfi_entry)) + PL_CFI_LEN_SIZE_64, @"Incorrect offset");
        STAssertEquals(fde_info.fde_length, (uint64_t)PL_CFI_SIZE_64, @"Incorrect length");
    } else {
        STAssertEquals(fde_info.fde_offset, (pl_vm_address_t) (sizeof(pl_cfi_entry)) + PL_CFI_LEN_SIZE_32, @"Incorrect offset");
        STAssertEquals(fde_info.fde_length, (uint64_t)PL_CFI_SIZE_32, @"Incorrect length");
    }

    //STAssertEquals(fde_info.fde_instruction_offset, (pl_vm_address_t)0x0, @"Incorrect instruction offset (should be the first entry)");
    
    plcrash_async_dwarf_fde_info_free(&fde_info);
    
    /* Verify that an unknown PC freturns ENOTFOUND */
    err = plcrash_async_dwarf_frame_reader_find_fde(&_debug_reader, 0x0, PL_CFI_DEBUG_FRAME_PC+PL_CFI_DEBUG_FRAME_PC_RANGE, &fde_info);
    STAssertEquals(PLCRASH_ENOTFOUND, err, @"FDE should not have been found");
}

@end
