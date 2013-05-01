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

#define EXTRACT_BITS(value, mask) ((value >> __builtin_ctz(mask)) & (((1 << __builtin_popcount(mask)))-1))
#define INSERT_BITS(bits, mask) ((bits << __builtin_ctz(mask)) & mask)

/**
 * Test handling of sparse register lists. These are only supported for the frame encodings; the 10-bit packed
 * encoding format does not support sparse lists.
 *
 * It's unclear as to whether these actually ever occur in the wild.
 */
- (void) testX86SparseRegisterDecoding {
    plcrash_async_cfe_entry_t entry;

    /* x86 handling */
    const uint32_t encoded_regs = UNWIND_X86_REG_ESI | (UNWIND_X86_REG_EDX << 3) | (UNWIND_X86_REG_ECX << 9);
    uint32_t encoding = UNWIND_X86_MODE_EBP_FRAME | INSERT_BITS(encoded_regs, UNWIND_X86_EBP_FRAME_REGISTERS);
    
    plcrash_error_t res = plcrash_async_cfe_entry_init(&entry, CPU_TYPE_X86, encoding);
    STAssertEquals(res, PLCRASH_ESUCCESS, @"Failed to decode entry");

    
    /* Extract the registers. Up to 5 may be encoded */
    plcrash_regnum_t expected_reg[] = {
        PLCRASH_X86_ESI,
        PLCRASH_X86_EDX,
        PLCRASH_REG_INVALID,
        PLCRASH_X86_ECX
    };
    
    uint32_t reg_count = plcrash_async_cfe_entry_register_count(&entry);
    STAssertEquals(reg_count, (uint32_t) (sizeof(expected_reg) / sizeof(expected_reg[0])), @"Incorrect register count extracted");
    
    plcrash_regnum_t reg[reg_count];
    plcrash_async_cfe_entry_register_list(&entry, reg);
    for (uint32_t i = 0; i < reg_count; i++) {
        STAssertEquals(reg[i], expected_reg[i], @"Incorrect register value extracted for position %" PRId32, i);
    }
    
    plcrash_async_cfe_entry_free(&entry);
}

/**
 * Decode an x86 EBP frame encoding.
 */
- (void) testX86DecodeFrame {

    /* Create a frame encoding, with registers saved at ebp-1020 bytes */
    const uint32_t encoded_reg_ebp_offset = 1020;
    const uint32_t encoded_regs = UNWIND_X86_REG_ESI |
        (UNWIND_X86_REG_EDX << 3) |
        (UNWIND_X86_REG_ECX << 6);

    uint32_t encoding = UNWIND_X86_MODE_EBP_FRAME |
        INSERT_BITS(encoded_reg_ebp_offset/4, UNWIND_X86_EBP_FRAME_OFFSET) |
        INSERT_BITS(encoded_regs, UNWIND_X86_EBP_FRAME_REGISTERS);

    /* Try decoding it */
    plcrash_async_cfe_entry_t entry;
    plcrash_error_t res = plcrash_async_cfe_entry_init(&entry, CPU_TYPE_X86, encoding);
    STAssertEquals(res, PLCRASH_ESUCCESS, @"Failed to decode entry");
    STAssertEquals(PLCRASH_ASYNC_CFE_ENTRY_TYPE_FRAME_PTR, plcrash_async_cfe_entry_type(&entry), @"Incorrect entry type");

    uint32 reg_ebp_offset = plcrash_async_cfe_entry_stack_offset(&entry);
    uint32_t reg_count = plcrash_async_cfe_entry_register_count(&entry);
    STAssertEquals(reg_ebp_offset, encoded_reg_ebp_offset, @"Incorrect offset extracted");
    STAssertEquals(reg_count, (uint32_t)3, @"Incorrect register count extracted");

    /* Extract the registers. Up to 5 may be encoded */
    plcrash_regnum_t expected_reg[] = {
        PLCRASH_X86_ESI,
        PLCRASH_X86_EDX,
        PLCRASH_X86_ECX
    };
    plcrash_regnum_t reg[reg_count];

    plcrash_async_cfe_entry_register_list(&entry, reg);
    for (uint32_t i = 0; i < 3; i++) {
        STAssertEquals(reg[i], expected_reg[i], @"Incorrect register value extracted for position %" PRId32, i);
    }
}

- (void) verifyFramelessRegDecode: (uint32_t) permutedRegisters
                            count: (uint32_t) count
                expectedRegisters: (const uint32_t[PLCRASH_ASYNC_CFE_SAVED_REGISTER_MAX]) expectedRegisters
{
    /* Verify that our encoder generates the same result */
    STAssertEquals(permutedRegisters, plcrash_async_cfe_register_encode(expectedRegisters, count), @"Incorrect internal encoding for count %" PRId32, count);

    /* Extract and verify the registers */
    uint32_t regs[count];
    plcrash_async_cfe_register_decode(permutedRegisters, count, regs);
    for (uint32_t i = 0; i < count; i++) {
        STAssertEquals(regs[i], expectedRegisters[i], @"Incorrect register value extracted for position %" PRId32, i);
    }
}

/**
 * Decode an x86 immediate 'frameless' encoding.
 */
- (void) testX86DecodeFramelessImmediate {
    /* Create a frame encoding, with registers saved at ebp-1020 bytes */
    const uint32_t encoded_stack_size = 1020;
    const uint32_t encoded_regs[] = { UNWIND_X86_REG_ESI, UNWIND_X86_REG_EDX, UNWIND_X86_REG_ECX };
    const uint32_t encoded_regs_count = sizeof(encoded_regs) / sizeof(encoded_regs[0]);
    const uint32_t encoded_regs_permutation = plcrash_async_cfe_register_encode(encoded_regs, encoded_regs_count);

    uint32_t encoding = UNWIND_X86_MODE_STACK_IMMD |
        INSERT_BITS(encoded_stack_size/4, UNWIND_X86_FRAMELESS_STACK_SIZE) |
        INSERT_BITS(encoded_regs_count, UNWIND_X86_FRAMELESS_STACK_REG_COUNT) |
        INSERT_BITS(encoded_regs_permutation, UNWIND_X86_FRAMELESS_STACK_REG_PERMUTATION);

    /* Try decoding it */
    plcrash_async_cfe_entry_t entry;
    plcrash_error_t res = plcrash_async_cfe_entry_init(&entry, CPU_TYPE_X86, encoding);
    STAssertEquals(res, PLCRASH_ESUCCESS, @"Failed to decode entry");
    STAssertEquals(PLCRASH_ASYNC_CFE_ENTRY_TYPE_FRAMELESS_IMMD, plcrash_async_cfe_entry_type(&entry), @"Incorrect entry type");

    uint32_t stack_size = plcrash_async_cfe_entry_stack_offset(&entry);
    uint32_t reg_count = plcrash_async_cfe_entry_register_count(&entry);

    STAssertEquals(stack_size, encoded_stack_size, @"Incorrect stack size decoded");
    STAssertEquals(reg_count, encoded_regs_count, @"Incorrect register count decoded");

    /* Verify the register decoding */
    plcrash_regnum_t reg[reg_count];

    plcrash_async_cfe_entry_register_list(&entry, reg);
    
    const plcrash_regnum_t expected_regs[] = { PLCRASH_X86_ESI, PLCRASH_X86_EDX, PLCRASH_X86_ECX };
    for (uint32_t i = 0; i < 3; i++) {
        STAssertEquals(reg[i], expected_regs[i], @"Incorrect register value extracted for position %" PRId32, i);
    }
}

/**
 * Decode an x86 indirect 'frameless' encoding.
 */
- (void) testX86DecodeFramelessIndirect {
    /* Create a frame encoding, with registers saved at ebp-1020 bytes */
    const uint32_t encoded_stack_size = 1020;
    const uint32_t encoded_regs[] = { UNWIND_X86_REG_ESI, UNWIND_X86_REG_EDX, UNWIND_X86_REG_ECX };
    const uint32_t encoded_regs_count = sizeof(encoded_regs) / sizeof(encoded_regs[0]);
    const uint32_t encoded_regs_permutation = plcrash_async_cfe_register_encode(encoded_regs, encoded_regs_count);
    
    uint32_t encoding = UNWIND_X86_MODE_STACK_IND |
    INSERT_BITS(encoded_stack_size/4, UNWIND_X86_FRAMELESS_STACK_SIZE) |
    INSERT_BITS(encoded_regs_count, UNWIND_X86_FRAMELESS_STACK_REG_COUNT) |
    INSERT_BITS(encoded_regs_permutation, UNWIND_X86_FRAMELESS_STACK_REG_PERMUTATION);
    
    /* Try decoding it */
    plcrash_async_cfe_entry_t entry;
    plcrash_error_t res = plcrash_async_cfe_entry_init(&entry, CPU_TYPE_X86, encoding);
    STAssertEquals(res, PLCRASH_ESUCCESS, @"Failed to decode entry");
    STAssertEquals(PLCRASH_ASYNC_CFE_ENTRY_TYPE_FRAMELESS_INDIRECT, plcrash_async_cfe_entry_type(&entry), @"Incorrect entry type");
    
    uint32_t stack_size = plcrash_async_cfe_entry_stack_offset(&entry);
    uint32_t reg_count = plcrash_async_cfe_entry_register_count(&entry);
    
    STAssertEquals(stack_size, encoded_stack_size, @"Incorrect stack size decoded");
    STAssertEquals(reg_count, encoded_regs_count, @"Incorrect register count decoded");
    
    /* Verify the register decoding */
    plcrash_regnum_t reg[reg_count];
    
    plcrash_async_cfe_entry_register_list(&entry, reg);
    
    const plcrash_regnum_t expected_regs[] = { PLCRASH_X86_ESI, PLCRASH_X86_EDX, PLCRASH_X86_ECX };
    for (uint32_t i = 0; i < 3; i++) {
        STAssertEquals(reg[i], expected_regs[i], @"Incorrect register value extracted for position %" PRId32, i);
    }
}

/**
 * Decode an x86 DWARF encoding.
 */
- (void) testX86DecodeDWARF {
    /* Create a frame encoding, with registers saved at ebp-1020 bytes */
    const uint32_t encoded_dwarf_offset = 1020;
    uint32_t encoding = UNWIND_X86_MODE_DWARF |
        INSERT_BITS(encoded_dwarf_offset, UNWIND_X86_DWARF_SECTION_OFFSET);

    /* Try decoding it */
    plcrash_async_cfe_entry_t entry;
    plcrash_error_t res = plcrash_async_cfe_entry_init(&entry, CPU_TYPE_X86, encoding);
    STAssertEquals(res, PLCRASH_ESUCCESS, @"Failed to decode entry");
    STAssertEquals(PLCRASH_ASYNC_CFE_ENTRY_TYPE_DWARF, plcrash_async_cfe_entry_type(&entry), @"Incorrect entry type");
    
    uint32_t dwarf_offset = plcrash_async_cfe_entry_stack_offset(&entry);    
    STAssertEquals(dwarf_offset, encoded_dwarf_offset, @"Incorrect dwarf offset decoded");
}

/**
 * Test handling of sparse register lists. These are only supported for the frame encodings; the 10-bit packed
 * encoding format does not support sparse lists.
 *
 * It's unclear as to whether these actually ever occur in the wild.
 */
- (void) testX86_64SparseRegisterDecoding {
    plcrash_async_cfe_entry_t entry;
    
    /* x86 handling */
    const uint32_t encoded_regs = UNWIND_X86_64_REG_RBX | (UNWIND_X86_64_REG_R12 << 3) | (UNWIND_X86_64_REG_R13 << 9);
    uint32_t encoding = UNWIND_X86_64_MODE_RBP_FRAME | INSERT_BITS(encoded_regs, UNWIND_X86_64_RBP_FRAME_REGISTERS);
    
    plcrash_error_t res = plcrash_async_cfe_entry_init(&entry, CPU_TYPE_X86_64, encoding);
    STAssertEquals(res, PLCRASH_ESUCCESS, @"Failed to decode entry");
    
    /* Extract the registers. Up to 5 may be encoded */
    plcrash_regnum_t expected_reg[] = {
        PLCRASH_X86_64_RBX,
        PLCRASH_X86_64_R12,
        PLCRASH_REG_INVALID,
        PLCRASH_X86_64_R13
    };
    
    uint32_t reg_count = plcrash_async_cfe_entry_register_count(&entry);
    STAssertEquals(reg_count, (uint32_t) (sizeof(expected_reg) / sizeof(expected_reg[0])), @"Incorrect register count extracted");
    
    plcrash_regnum_t reg[reg_count];
    plcrash_async_cfe_entry_register_list(&entry, reg);
    for (uint32_t i = 0; i < reg_count; i++) {
        STAssertEquals(reg[i], expected_reg[i], @"Incorrect register value extracted for position %" PRId32, i);
    }
    
    plcrash_async_cfe_entry_free(&entry);
}

/**
 * Decode an x86-64 RBP frame encoding.
 */
- (void) testX86_64DecodeFrame {
    /* Create a frame encoding, with registers saved at rbp-1020 bytes */
    const uint32_t encoded_reg_rbp_offset = 1016;
    const uint32_t encoded_regs = UNWIND_X86_64_REG_R12 |
        (UNWIND_X86_64_REG_R13 << 3) |
        (UNWIND_X86_64_REG_R14 << 6);
    
    uint32_t encoding = UNWIND_X86_64_MODE_RBP_FRAME |
        INSERT_BITS(encoded_reg_rbp_offset/8, UNWIND_X86_64_RBP_FRAME_OFFSET) |
        INSERT_BITS(encoded_regs, UNWIND_X86_64_RBP_FRAME_REGISTERS);
    
    /* Try decoding it */
    plcrash_async_cfe_entry_t entry;
    plcrash_error_t res = plcrash_async_cfe_entry_init(&entry, CPU_TYPE_X86_64, encoding);
    STAssertEquals(res, PLCRASH_ESUCCESS, @"Failed to decode entry");
    STAssertEquals(PLCRASH_ASYNC_CFE_ENTRY_TYPE_FRAME_PTR, plcrash_async_cfe_entry_type(&entry), @"Incorrect entry type");
    
    uint32 reg_ebp_offset = plcrash_async_cfe_entry_stack_offset(&entry);
    uint32_t reg_count = plcrash_async_cfe_entry_register_count(&entry);
    STAssertEquals(reg_ebp_offset, encoded_reg_rbp_offset, @"Incorrect offset extracted");
    STAssertEquals(reg_count, (uint32_t)3, @"Incorrect register count extracted");

    /* Extract the registers. Up to 5 may be encoded */
    plcrash_regnum_t expected_reg[] = {
        PLCRASH_X86_64_R12,
        PLCRASH_X86_64_R13,
        PLCRASH_X86_64_R14
    };
    plcrash_regnum_t reg[reg_count];
    
    plcrash_async_cfe_entry_register_list(&entry, reg);
    for (uint32_t i = 0; i < 3; i++) {
        STAssertEquals(reg[i], expected_reg[i], @"Incorrect register value extracted for position %" PRId32, i);
    }
}

/**
 * Decode an x86-64 immediate 'frameless' encoding.
 */
- (void) testX86_64DecodeFramelessImmediate {
    /* Create a frame encoding, with registers saved at ebp-1020 bytes */
    const uint32_t encoded_stack_size = 1016;
    const uint32_t encoded_regs[] = { UNWIND_X86_64_REG_R12, UNWIND_X86_64_REG_R13, UNWIND_X86_64_REG_R14 };
    const uint32_t encoded_regs_count = sizeof(encoded_regs) / sizeof(encoded_regs[0]);
    const uint32_t encoded_regs_permutation = plcrash_async_cfe_register_encode(encoded_regs, encoded_regs_count);
    
    uint32_t encoding = UNWIND_X86_64_MODE_STACK_IMMD |
    INSERT_BITS(encoded_stack_size/8, UNWIND_X86_64_FRAMELESS_STACK_SIZE) |
    INSERT_BITS(encoded_regs_count, UNWIND_X86_64_FRAMELESS_STACK_REG_COUNT) |
    INSERT_BITS(encoded_regs_permutation, UNWIND_X86_64_FRAMELESS_STACK_REG_PERMUTATION);
    
    /* Try decoding it */
    plcrash_async_cfe_entry_t entry;
    plcrash_error_t res = plcrash_async_cfe_entry_init(&entry, CPU_TYPE_X86_64, encoding);
    STAssertEquals(res, PLCRASH_ESUCCESS, @"Failed to decode entry");
    STAssertEquals(PLCRASH_ASYNC_CFE_ENTRY_TYPE_FRAMELESS_IMMD, plcrash_async_cfe_entry_type(&entry), @"Incorrect entry type");
    
    uint32_t stack_size = plcrash_async_cfe_entry_stack_offset(&entry);
    uint32_t reg_count = plcrash_async_cfe_entry_register_count(&entry);
    
    STAssertEquals(stack_size, encoded_stack_size, @"Incorrect stack size decoded");
    STAssertEquals(reg_count, encoded_regs_count, @"Incorrect register count decoded");
    
    /* Verify the register decoding */
    plcrash_regnum_t reg[reg_count];
    
    plcrash_async_cfe_entry_register_list(&entry, reg);
    
    const plcrash_regnum_t expected_regs[] = { PLCRASH_X86_64_R12, PLCRASH_X86_64_R13, PLCRASH_X86_64_R14 };
    for (uint32_t i = 0; i < 3; i++) {
        STAssertEquals(reg[i], expected_regs[i], @"Incorrect register value extracted for position %" PRId32, i);
    }
}

/**
 * Decode an x86-64 indirect 'frameless' encoding.
 */
- (void) testX86_64DecodeFramelessIndirect {
    /* Create a frame encoding, with registers saved at ebp-1020 bytes */
    const uint32_t encoded_stack_size = 1016;
    const uint32_t encoded_regs[] = { UNWIND_X86_64_REG_R12, UNWIND_X86_64_REG_R13, UNWIND_X86_64_REG_R14 };
    const uint32_t encoded_regs_count = sizeof(encoded_regs) / sizeof(encoded_regs[0]);
    const uint32_t encoded_regs_permutation = plcrash_async_cfe_register_encode(encoded_regs, encoded_regs_count);
    
    uint32_t encoding = UNWIND_X86_64_MODE_STACK_IND |
    INSERT_BITS(encoded_stack_size/8, UNWIND_X86_64_FRAMELESS_STACK_SIZE) |
    INSERT_BITS(encoded_regs_count, UNWIND_X86_64_FRAMELESS_STACK_REG_COUNT) |
    INSERT_BITS(encoded_regs_permutation, UNWIND_X86_64_FRAMELESS_STACK_REG_PERMUTATION);
    
    /* Try decoding it */
    plcrash_async_cfe_entry_t entry;
    plcrash_error_t res = plcrash_async_cfe_entry_init(&entry, CPU_TYPE_X86_64, encoding);
    STAssertEquals(res, PLCRASH_ESUCCESS, @"Failed to decode entry");
    STAssertEquals(PLCRASH_ASYNC_CFE_ENTRY_TYPE_FRAMELESS_INDIRECT, plcrash_async_cfe_entry_type(&entry), @"Incorrect entry type");
    
    uint32_t stack_size = plcrash_async_cfe_entry_stack_offset(&entry);
    uint32_t reg_count = plcrash_async_cfe_entry_register_count(&entry);
    
    STAssertEquals(stack_size, encoded_stack_size, @"Incorrect stack size decoded");
    STAssertEquals(reg_count, encoded_regs_count, @"Incorrect register count decoded");
    
    /* Verify the register decoding */
    plcrash_regnum_t reg[reg_count];
    
    plcrash_async_cfe_entry_register_list(&entry, reg);
    
    const plcrash_regnum_t expected_regs[] = { PLCRASH_X86_64_R12, PLCRASH_X86_64_R13, PLCRASH_X86_64_R14 };
    for (uint32_t i = 0; i < 3; i++) {
        STAssertEquals(reg[i], expected_regs[i], @"Incorrect register value extracted for position %" PRId32, i);
    }
}

/**
 * Decode an x86-64 DWARF encoding.
 */
- (void) testX86_64DecodeDWARF {
    /* Create a frame encoding, with registers saved at ebp-1020 bytes */
    const uint32_t encoded_dwarf_offset = 1016;
    uint32_t encoding = UNWIND_X86_64_MODE_DWARF |
        INSERT_BITS(encoded_dwarf_offset, UNWIND_X86_64_DWARF_SECTION_OFFSET);
    
    /* Try decoding it */
    plcrash_async_cfe_entry_t entry;
    plcrash_error_t res = plcrash_async_cfe_entry_init(&entry, CPU_TYPE_X86_64, encoding);
    STAssertEquals(res, PLCRASH_ESUCCESS, @"Failed to decode entry");
    STAssertEquals(PLCRASH_ASYNC_CFE_ENTRY_TYPE_DWARF, plcrash_async_cfe_entry_type(&entry), @"Incorrect entry type");
    
    uint32_t dwarf_offset = plcrash_async_cfe_entry_stack_offset(&entry);    
    STAssertEquals(dwarf_offset, encoded_dwarf_offset, @"Incorrect dwarf offset decoded");
}

/**
 * Verify encoding+decoding of permuted frameless registers, verifying all supported lengths. The test cases were
 * all extracted from code generated by clang.
 */
- (void) testPermutedRegisterEncoding {
#define PL_EXBIT(v) EXTRACT_BITS(v, UNWIND_X86_64_FRAMELESS_STACK_REG_PERMUTATION)
    /* 1 item */
    {
        const uint32_t expected[PLCRASH_ASYNC_CFE_SAVED_REGISTER_MAX] = { UNWIND_X86_64_REG_RBX };
        [self verifyFramelessRegDecode: PL_EXBIT(0x02020400) count: 1 expectedRegisters: expected];
    }

    /* 2 items */
    {
        const uint32_t expected[PLCRASH_ASYNC_CFE_SAVED_REGISTER_MAX] = {
            UNWIND_X86_64_REG_R15,
            UNWIND_X86_64_REG_R14
        };
        [self verifyFramelessRegDecode: PL_EXBIT(0x02030817) count: 2 expectedRegisters: expected];
    }

    /* 3 items */
    {
        const uint32_t expected[PLCRASH_ASYNC_CFE_SAVED_REGISTER_MAX] = {
            UNWIND_X86_64_REG_RBX,
            UNWIND_X86_64_REG_R14,
            UNWIND_X86_64_REG_R15
        };
        [self verifyFramelessRegDecode: PL_EXBIT(0x02040C0A) count: 3 expectedRegisters: expected];
    }

    /* 4 items */
    {
        const uint32_t expected[PLCRASH_ASYNC_CFE_SAVED_REGISTER_MAX] = {
            UNWIND_X86_64_REG_RBX,
            UNWIND_X86_64_REG_R12,
            UNWIND_X86_64_REG_R14,
            UNWIND_X86_64_REG_R15
        };
        [self verifyFramelessRegDecode: PL_EXBIT(0x02051004) count: 4 expectedRegisters: expected];
    }

    /* 5 items */
    {
        const uint32_t expected[PLCRASH_ASYNC_CFE_SAVED_REGISTER_MAX] = {
            UNWIND_X86_64_REG_RBX,
            UNWIND_X86_64_REG_R12,
            UNWIND_X86_64_REG_R13,
            UNWIND_X86_64_REG_R14,
            UNWIND_X86_64_REG_R15
        };
        [self verifyFramelessRegDecode: PL_EXBIT(0x02071800) count: 5 expectedRegisters: expected];
    }

    /* 6 items */
    {
        const uint32_t expected[PLCRASH_ASYNC_CFE_SAVED_REGISTER_MAX] = {
            UNWIND_X86_64_REG_RBX,
            UNWIND_X86_64_REG_R12,
            UNWIND_X86_64_REG_R13,
            UNWIND_X86_64_REG_R14,
            UNWIND_X86_64_REG_R15,
            UNWIND_X86_64_REG_RBP
        };
        [self verifyFramelessRegDecode: PL_EXBIT(0x02071800) count: 6 expectedRegisters: expected];
    }
#undef PL_EXBIT
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
