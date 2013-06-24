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


#import "PLCrashTestCase.h"

#import "PLCrashAsyncCompactUnwindEncoding.h"
#import "PLCrashAsyncMachOImage.h"

#import <TargetConditionals.h>

#import <mach-o/fat.h>
#import <mach-o/arch.h>
#import <mach-o/dyld.h>

#if TARGET_OS_MAC && (!TARGET_OS_IPHONE)
#define TEST_BINARY @"test.macosx"
#elif TARGET_IPHONE_SIMULATOR
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
@interface PLCrashAsyncCompactUnwindEncodingTests : PLCrashTestCase {
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
    
    /* Load and parse the Mach-o image. */
    plcrash_error_t err;
    NSData *mappedImage = [self nativeBinaryFromTestResource: TEST_BINARY];
    
    err = plcrash_nasync_macho_init(&_image, mach_task_self(), [TEST_BINARY UTF8String], [mappedImage bytes]);
    STAssertEquals(err, PLCRASH_ESUCCESS, @"Failed to initialize Mach-O parser");
    
    /* Map the unwind section */
    err = plcrash_async_macho_map_section(&_image, SEG_TEXT, "__unwind_info", &_unwind_mobj);
    STAssertEquals(err, PLCRASH_ESUCCESS, @"Failed to map unwind info");
    
    
    /* Initialize the CFE reader */
#if !defined(__i386__) || !defined(__x86_64__)
    /* CFE is currently only supported for x86/x86-64, but our target binaries are not architecture specific;
     * we fudge the type reported to the reader to allow us to test the reader on ARM anyway. */
    cpu_type_t cputype = CPU_TYPE_X86;
#else
    cpu_type_t cputype = _image.byteorder->swap32(_image.header.cputype);
#endif
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
 * Test handling of NULL encoding.
 */
- (void) testX86DecodeNULLEncoding {
    plcrash_async_cfe_entry_t entry;
    STAssertEquals(plcrash_async_cfe_entry_init(&entry, CPU_TYPE_X86, 0x0), PLCRASH_ESUCCESS, @"Should return NOTFOUND for NULL encoding");
    STAssertEquals(plcrash_async_cfe_entry_type(&entry), PLCRASH_ASYNC_CFE_ENTRY_TYPE_NONE, @"Incorrect CFE type");
}

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

    uint32_t reg_ebp_offset = plcrash_async_cfe_entry_stack_offset(&entry);
    uint32_t reg_count = plcrash_async_cfe_entry_register_count(&entry);
    STAssertEquals(reg_ebp_offset, -encoded_reg_ebp_offset, @"Incorrect offset extracted");
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
    
    plcrash_async_cfe_entry_free(&entry);
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
    
    plcrash_async_cfe_entry_free(&entry);
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
    
    plcrash_async_cfe_entry_free(&entry);
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
    
    plcrash_async_cfe_entry_free(&entry);
}

/**
 * Test handling of NULL encoding.
 */
- (void) testX86_64DecodeNULLEncoding {
    plcrash_async_cfe_entry_t entry;
    STAssertEquals(plcrash_async_cfe_entry_init(&entry, CPU_TYPE_X86_64, 0x0), PLCRASH_ESUCCESS, @"Should return NOTFOUND for NULL encoding");
    STAssertEquals(plcrash_async_cfe_entry_type(&entry), PLCRASH_ASYNC_CFE_ENTRY_TYPE_NONE, @"Incorrect CFE type");
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
    
    uint32_t reg_ebp_offset = plcrash_async_cfe_entry_stack_offset(&entry);
    uint32_t reg_count = plcrash_async_cfe_entry_register_count(&entry);
    STAssertEquals(reg_ebp_offset, -encoded_reg_rbp_offset, @"Incorrect offset extracted");
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
    
    plcrash_async_cfe_entry_free(&entry);
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
    
    plcrash_async_cfe_entry_free(&entry);
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
    
    plcrash_async_cfe_entry_free(&entry);
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
    
    plcrash_async_cfe_entry_free(&entry);
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
    pl_vm_address_t function_base;
    plcrash_error_t err;

    uint32_t encoding;
    err = plcrash_async_cfe_reader_find_pc(&_reader, PC_COMPACT_COMMON, &function_base, &encoding);
    STAssertEquals(PLCRASH_ESUCCESS, err, @"Failed to locate CFE entry");
    STAssertEquals(function_base, (pl_vm_address_t)PC_COMPACT_COMMON, @"Incorrect function base returned");
    STAssertEquals(encoding, (uint32_t)PC_COMPACT_COMMON_ENCODING, @"Incorrect encoding returned");
}

/**
 * Test reading of a PC, compressed, with a private encoding.
 */
- (void) testReadCompressedEncoding {
    pl_vm_address_t function_base;
    plcrash_error_t err;
    
    uint32_t encoding;
    err = plcrash_async_cfe_reader_find_pc(&_reader, PC_COMPACT_PRIVATE, &function_base, &encoding);
    STAssertEquals(PLCRASH_ESUCCESS, err, @"Failed to locate CFE entry");
    STAssertEquals(function_base, (pl_vm_address_t)PC_COMPACT_PRIVATE, @"Incorrect function base returned");
    STAssertEquals(encoding, (uint32_t)PC_COMPACT_PRIVATE_ENCODING, @"Incorrect encoding returned");
}

/**
 * Test reading of a PC, regular, with a common encoding.
 */
- (void) testReadRegularEncoding {
    pl_vm_address_t function_base;
    plcrash_error_t err;
    
    uint32_t encoding;
    err = plcrash_async_cfe_reader_find_pc(&_reader, PC_REGULAR, &function_base, &encoding);
    STAssertEquals(PLCRASH_ESUCCESS, err, @"Failed to locate CFE entry");
    STAssertEquals(function_base, (pl_vm_address_t)PC_REGULAR, @"Incorrect function base returned");
    STAssertEquals(encoding, (uint32_t)PC_REGULAR_ENCODING, @"Incorrect encoding returned");
}


/*
 * CFE is only supported on x86/x86-64, and the iOS SDK does not provide the thread state APIs necessary
 * to perform these tests on ARM
 *
 * TODO: Disable the entire CFE subsystem on ARM, eg, via a configuration system for enabling/disabling
 * functionality.
 */
#if PLCRASH_ASYNC_THREAD_X86_SUPPORT
- (void) testx86_64_ApplyFramePTRState {
    plcrash_async_cfe_entry_t entry;
    plcrash_async_thread_state_t ts;
    
    /* Set up a faux frame */
    uint64_t stackframe[] = {
        12, // r12
        13, // r13
        0,  // sparse slot
        14, // r14

        1,  // rbp
        2,  // ret addr
    };

    /* Create a frame encoding, with registers saved at rbp-32 bytes. We insert
     * a sparse slot to test the sparse handling */
    const uint32_t encoded_reg_rbp_offset = 32;
    const uint32_t encoded_regs = UNWIND_X86_64_REG_R12 |
        (UNWIND_X86_64_REG_R13 << 3) |
        0 << 6 /* SPARSE */ |
        (UNWIND_X86_64_REG_R14 << 9);
    
    uint32_t encoding = UNWIND_X86_64_MODE_RBP_FRAME |
    INSERT_BITS(encoded_reg_rbp_offset/8, UNWIND_X86_64_RBP_FRAME_OFFSET) |
    INSERT_BITS(encoded_regs, UNWIND_X86_64_RBP_FRAME_REGISTERS);
    
    STAssertEquals(PLCRASH_ESUCCESS, plcrash_async_cfe_entry_init(&entry, CPU_TYPE_X86_64, encoding), @"Failed to initialize CFE entry");
    
    /* Initialize default thread state */
    plcrash_greg_t stack_addr = &stackframe[4]; // rbp
    STAssertEquals(plcrash_async_thread_state_init(&ts, CPU_TYPE_X86_64), PLCRASH_ESUCCESS, @"Failed to initialize thread state");
    plcrash_async_thread_state_set_reg(&ts, PLCRASH_REG_FP, stack_addr);

    /* Apply! */
    plcrash_async_thread_state_t nts;
    plcrash_error_t err = plcrash_async_cfe_entry_apply(mach_task_self(), 0x0, &ts, &entry, &nts);
    STAssertEquals(err, PLCRASH_ESUCCESS, @"Failed to apply state to thread");
    
    /* Verify! */
    STAssertTrue(plcrash_async_thread_state_has_reg(&nts, PLCRASH_X86_64_RSP), @"Missing expected register");
    STAssertTrue(plcrash_async_thread_state_has_reg(&nts, PLCRASH_X86_64_RBP), @"Missing expected register");
    STAssertTrue(plcrash_async_thread_state_has_reg(&nts, PLCRASH_X86_64_RIP), @"Missing expected register");

    STAssertTrue(plcrash_async_thread_state_has_reg(&nts, PLCRASH_X86_64_R12), @"Missing expected register");
    STAssertTrue(plcrash_async_thread_state_has_reg(&nts, PLCRASH_X86_64_R13), @"Missing expected register");
    STAssertTrue(plcrash_async_thread_state_has_reg(&nts, PLCRASH_X86_64_R14), @"Missing expected register");

    STAssertEquals(plcrash_async_thread_state_get_reg(&nts, PLCRASH_X86_64_RSP), (plcrash_greg_t)stack_addr+(16), @"Incorrect register value");
    STAssertEquals(plcrash_async_thread_state_get_reg(&nts, PLCRASH_X86_64_RBP), (plcrash_greg_t)1, @"Incorrect register value");
    STAssertEquals(plcrash_async_thread_state_get_reg(&nts, PLCRASH_X86_64_RIP), (plcrash_greg_t)2, @"Incorrect register value");
    
    STAssertEquals(plcrash_async_thread_state_get_reg(&nts, PLCRASH_X86_64_R12), (plcrash_greg_t)12, @"Incorrect register value");
    STAssertEquals(plcrash_async_thread_state_get_reg(&nts, PLCRASH_X86_64_R13), (plcrash_greg_t)13, @"Incorrect register value");
    STAssertEquals(plcrash_async_thread_state_get_reg(&nts, PLCRASH_X86_64_R14), (plcrash_greg_t)14, @"Incorrect register value");
    
    plcrash_async_cfe_entry_free(&entry);
}

/* This test requires storing local pointers to the host's stack in 32-bit x86's thread state; it can only be run on a 32-bit host,
 * as the 64-bit stack pointers may exceed the UINT32_MAX. */
#ifndef __LP64__
- (void) testx86_32_ApplyFramePTRState {
    plcrash_async_cfe_entry_t entry;
    plcrash_async_thread_state_t ts;
    
    /* Set up a faux frame */
    uint32_t stackframe[] = {
        12, // ebx
        13, // ecx
        0,  // sparse slot
        14, // edi
        
        1,  // ebp
        2,  // ret addr
    };
    
    /* Create a frame encoding, with registers saved at ebp-16 bytes. We insert
     * a sparse slot to test the sparse handling */
    const uint32_t encoded_reg_ebp_offset = 16;
    const uint32_t encoded_regs = UNWIND_X86_REG_EBX |
                                    (UNWIND_X86_REG_ECX << 3) |
                                    0 << 6 /* SPARSE */ |
                                    (UNWIND_X86_REG_EDI << 9);
    
    uint32_t encoding = UNWIND_X86_MODE_EBP_FRAME |
    INSERT_BITS(encoded_reg_ebp_offset/4, UNWIND_X86_EBP_FRAME_OFFSET) |
    INSERT_BITS(encoded_regs, UNWIND_X86_EBP_FRAME_REGISTERS);
    
    STAssertEquals(PLCRASH_ESUCCESS, plcrash_async_cfe_entry_init(&entry, CPU_TYPE_X86, encoding), @"Failed to initialize CFE entry");
    
    /* Initialize default thread state */
    plcrash_greg_t stack_addr = &stackframe[4]; // ebp
    STAssertEquals(plcrash_async_thread_state_init(&ts, CPU_TYPE_X86), PLCRASH_ESUCCESS, @"Failed to initialize thread state");
    plcrash_async_thread_state_set_reg(&ts, PLCRASH_REG_FP, stack_addr);
    
    /* Apply! */
    plcrash_async_thread_state_t nts;
    plcrash_error_t err = plcrash_async_cfe_entry_apply(mach_task_self(), 0x0, &ts, &entry, &nts);
    STAssertEquals(err, PLCRASH_ESUCCESS, @"Failed to apply state to thread");
    
    /* Verify! */
    STAssertTrue(plcrash_async_thread_state_has_reg(&nts, PLCRASH_X86_ESP), @"Missing expected register");
    STAssertTrue(plcrash_async_thread_state_has_reg(&nts, PLCRASH_X86_EBP), @"Missing expected register");
    STAssertTrue(plcrash_async_thread_state_has_reg(&nts, PLCRASH_X86_EIP), @"Missing expected register");
    
    STAssertTrue(plcrash_async_thread_state_has_reg(&nts, PLCRASH_X86_EBX), @"Missing expected register");
    STAssertTrue(plcrash_async_thread_state_has_reg(&nts, PLCRASH_X86_ECX), @"Missing expected register");
    STAssertTrue(plcrash_async_thread_state_has_reg(&nts, PLCRASH_X86_EDI), @"Missing expected register");
    
    STAssertEquals(plcrash_async_thread_state_get_reg(&nts, PLCRASH_X86_ESP), (plcrash_greg_t)stack_addr+(8), @"Incorrect register value");
    STAssertEquals(plcrash_async_thread_state_get_reg(&nts, PLCRASH_X86_EBP), (plcrash_greg_t)1, @"Incorrect register value");
    STAssertEquals(plcrash_async_thread_state_get_reg(&nts, PLCRASH_X86_EIP), (plcrash_greg_t)2, @"Incorrect register value");
    
    STAssertEquals(plcrash_async_thread_state_get_reg(&nts, PLCRASH_X86_EBX), (plcrash_greg_t)12, @"Incorrect register value");
    STAssertEquals(plcrash_async_thread_state_get_reg(&nts, PLCRASH_X86_ECX), (plcrash_greg_t)13, @"Incorrect register value");
    STAssertEquals(plcrash_async_thread_state_get_reg(&nts, PLCRASH_X86_EDI), (plcrash_greg_t)14, @"Incorrect register value");
    
    plcrash_async_cfe_entry_free(&entry);
}
#endif /* !__LP64__ */

/**
 * Apply an x86-64 immediate 'frameless' encoding.
 */
- (void) testX86_64_ApplyFramePTRState_IMD {
    plcrash_async_cfe_entry_t entry;
    plcrash_async_thread_state_t ts;

    /* Set up a faux frame */
    uint64_t stackframe[] = {
        10, // rbp
        12, // r12
        13, // r13
        14, // r14

        2,  // ret addr
    };

    /* Create a frame encoding, with registers saved at esp-32 bytes */
    const uint32_t encoded_stack_size = 32;
    const uint32_t encoded_regs[] = { UNWIND_X86_64_REG_RBP, UNWIND_X86_64_REG_R12, UNWIND_X86_64_REG_R13, UNWIND_X86_64_REG_R14 };
    const uint32_t encoded_regs_count = sizeof(encoded_regs) / sizeof(encoded_regs[0]);
    const uint32_t encoded_regs_permutation = plcrash_async_cfe_register_encode(encoded_regs, encoded_regs_count);
    
    uint32_t encoding = UNWIND_X86_64_MODE_STACK_IMMD |
    INSERT_BITS(encoded_stack_size/8, UNWIND_X86_64_FRAMELESS_STACK_SIZE) |
    INSERT_BITS(encoded_regs_count, UNWIND_X86_64_FRAMELESS_STACK_REG_COUNT) |
    INSERT_BITS(encoded_regs_permutation, UNWIND_X86_64_FRAMELESS_STACK_REG_PERMUTATION);
    
    STAssertEquals(plcrash_async_cfe_entry_init(&entry, CPU_TYPE_X86_64, encoding), PLCRASH_ESUCCESS, @"Failed to decode entry");
    
    /* Initialize default thread state */
    plcrash_greg_t stack_addr = &stackframe[4]; // return address
    STAssertEquals(plcrash_async_thread_state_init(&ts, CPU_TYPE_X86_64), PLCRASH_ESUCCESS, @"Failed to initialize thread state");
    plcrash_async_thread_state_set_reg(&ts, PLCRASH_REG_SP, stack_addr - encoded_stack_size);

    /* Apply */
    plcrash_async_thread_state_t nts;
    plcrash_error_t err = plcrash_async_cfe_entry_apply(mach_task_self(), 0x0, &ts, &entry, &nts);
    STAssertEquals(err, PLCRASH_ESUCCESS, @"Failed to apply state to thread");
    
    /* Verify */
    STAssertTrue(plcrash_async_thread_state_has_reg(&nts, PLCRASH_X86_64_RSP), @"Missing expected register");
    STAssertTrue(plcrash_async_thread_state_has_reg(&nts, PLCRASH_X86_64_RIP), @"Missing expected register");
    
    STAssertTrue(plcrash_async_thread_state_has_reg(&nts, PLCRASH_X86_64_RBP), @"Missing expected register");
    STAssertTrue(plcrash_async_thread_state_has_reg(&nts, PLCRASH_X86_64_R12), @"Missing expected register");
    STAssertTrue(plcrash_async_thread_state_has_reg(&nts, PLCRASH_X86_64_R13), @"Missing expected register");
    STAssertTrue(plcrash_async_thread_state_has_reg(&nts, PLCRASH_X86_64_R14), @"Missing expected register");
    
    STAssertEquals(plcrash_async_thread_state_get_reg(&nts, PLCRASH_X86_64_RSP), (plcrash_greg_t)stack_addr+8, @"Incorrect register value");
    STAssertEquals(plcrash_async_thread_state_get_reg(&nts, PLCRASH_X86_64_RIP), (plcrash_greg_t)2, @"Incorrect register value");
    
    STAssertEquals(plcrash_async_thread_state_get_reg(&nts, PLCRASH_X86_64_RBP), (plcrash_greg_t)10, @"Incorrect register value");
    STAssertEquals(plcrash_async_thread_state_get_reg(&nts, PLCRASH_X86_64_R12), (plcrash_greg_t)12, @"Incorrect register value");
    STAssertEquals(plcrash_async_thread_state_get_reg(&nts, PLCRASH_X86_64_R13), (plcrash_greg_t)13, @"Incorrect register value");
    STAssertEquals(plcrash_async_thread_state_get_reg(&nts, PLCRASH_X86_64_R14), (plcrash_greg_t)14, @"Incorrect register value");
    
    plcrash_async_cfe_entry_free(&entry);
}

/**
 * Apply an x86-64 indirect 'frameless' encoding.
 */
- (void) testX86_64_ApplyFramePTRState_IND {
    plcrash_async_cfe_entry_t entry;
    plcrash_async_thread_state_t ts;

    /* Set up a faux frame */
    uint64_t stackframe[] = {
        10, // rbp
        12, // r12
        13, // r13
        14, // r14
        
        2,  // ret addr
    };
    

    /* Create a frame encoding */
    const uint32_t encoded_stack_size = 128;
    const uint32_t encoded_regs[] = { UNWIND_X86_64_REG_RBP, UNWIND_X86_64_REG_R12, UNWIND_X86_64_REG_R13, UNWIND_X86_64_REG_R14 };
    const uint32_t encoded_regs_count = sizeof(encoded_regs) / sizeof(encoded_regs[0]);
    const uint32_t encoded_regs_permutation = plcrash_async_cfe_register_encode(encoded_regs, encoded_regs_count);
    
    /* Indirect address target */
    uint32_t indirect_encoded_stack_size = 32;
    pl_vm_address_t function_address = ((pl_vm_address_t) &indirect_encoded_stack_size) - encoded_stack_size;
    
    uint32_t encoding = UNWIND_X86_64_MODE_STACK_IND |
    INSERT_BITS(encoded_stack_size/8, UNWIND_X86_64_FRAMELESS_STACK_SIZE) |
    INSERT_BITS(encoded_regs_count, UNWIND_X86_64_FRAMELESS_STACK_REG_COUNT) |
    INSERT_BITS(encoded_regs_permutation, UNWIND_X86_64_FRAMELESS_STACK_REG_PERMUTATION);
    
    STAssertEquals(plcrash_async_cfe_entry_init(&entry, CPU_TYPE_X86_64, encoding), PLCRASH_ESUCCESS, @"Failed to decode entry");
    
    /* Initialize default thread state */
    plcrash_greg_t stack_addr = &stackframe[4]; // return address
    STAssertEquals(plcrash_async_thread_state_init(&ts, CPU_TYPE_X86_64), PLCRASH_ESUCCESS, @"Failed to initialize thread state");
    plcrash_async_thread_state_set_reg(&ts, PLCRASH_REG_SP, stack_addr - indirect_encoded_stack_size);
    
    /* Apply */
    plcrash_async_thread_state_t nts;
    plcrash_error_t err = plcrash_async_cfe_entry_apply(mach_task_self(), function_address, &ts, &entry, &nts);
    STAssertEquals(err, PLCRASH_ESUCCESS, @"Failed to apply state to thread");
    
    /* Verify */
    STAssertTrue(plcrash_async_thread_state_has_reg(&nts, PLCRASH_X86_64_RSP), @"Missing expected register");
    STAssertTrue(plcrash_async_thread_state_has_reg(&nts, PLCRASH_X86_64_RIP), @"Missing expected register");
    
    STAssertTrue(plcrash_async_thread_state_has_reg(&nts, PLCRASH_X86_64_RBP), @"Missing expected register");
    STAssertTrue(plcrash_async_thread_state_has_reg(&nts, PLCRASH_X86_64_R12), @"Missing expected register");
    STAssertTrue(plcrash_async_thread_state_has_reg(&nts, PLCRASH_X86_64_R13), @"Missing expected register");
    STAssertTrue(plcrash_async_thread_state_has_reg(&nts, PLCRASH_X86_64_R14), @"Missing expected register");
    
    STAssertEquals(plcrash_async_thread_state_get_reg(&nts, PLCRASH_X86_64_RSP), (plcrash_greg_t)stack_addr+8, @"Incorrect register value");
    STAssertEquals(plcrash_async_thread_state_get_reg(&nts, PLCRASH_X86_64_RIP), (plcrash_greg_t)2, @"Incorrect register value");
    
    STAssertEquals(plcrash_async_thread_state_get_reg(&nts, PLCRASH_X86_64_RBP), (plcrash_greg_t)10, @"Incorrect register value");
    STAssertEquals(plcrash_async_thread_state_get_reg(&nts, PLCRASH_X86_64_R12), (plcrash_greg_t)12, @"Incorrect register value");
    STAssertEquals(plcrash_async_thread_state_get_reg(&nts, PLCRASH_X86_64_R13), (plcrash_greg_t)13, @"Incorrect register value");
    STAssertEquals(plcrash_async_thread_state_get_reg(&nts, PLCRASH_X86_64_R14), (plcrash_greg_t)14, @"Incorrect register value");
    
    plcrash_async_cfe_entry_free(&entry);
}

#endif /* PLCRASH_ASYNC_THREAD_X86_SUPPORT */

@end
