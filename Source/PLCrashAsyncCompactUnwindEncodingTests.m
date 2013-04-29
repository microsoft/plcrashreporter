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

    /* Try extracting it */
    uint32 reg_ebp_offset = EXTRACT_BITS(encoding, UNWIND_X86_EBP_FRAME_OFFSET) * 4;
    uint32 regs = EXTRACT_BITS(encoding, UNWIND_X86_EBP_FRAME_REGISTERS);
    
    STAssertEquals(reg_ebp_offset, encoded_reg_ebp_offset, @"Incorrect offset extracted");
    STAssertEquals(regs, encoded_regs, @"Incorrect register list extracted");

    /* Extract the registers. Up to 5 may be encoded */
    uint32_t expected_reg[6] = {
        UNWIND_X86_REG_ESI,
        UNWIND_X86_REG_EDX,
        UNWIND_X86_REG_ECX,
        UNWIND_X86_REG_NONE,
        UNWIND_X86_REG_NONE
    };
    for (uint32_t i = 0; i < sizeof(expected_reg) / expected_reg[0]; i++) {
        uint32_t reg = (regs >> (3 * i)) & 0x7;
        STAssertEquals(reg, expected_reg[i], @"Incorrect register value extracted for position %" PRId32, i);
    }
}

#define SAVED_REGISTER_MAX 6

/**
 * Encode a 10 bit ordered register list.
 */
static uint32_t reg_permute_encode(const uint32_t registers[SAVED_REGISTER_MAX], uint32_t count) {
    /*
     * Use a positional encoding to encode each integer in the list as an integer value
     * that is less than the previous greatest integer in the list. We know that each
     * integer (numbered 1-6) may appear only once in the list.
     *
     * For example:
     *   6 5 4 3 2 1 ->
     *   5 4 3 2 1 0
     *
     *   6 3 5 2 1 ->
     *   5 2 3 1 0
     *
     *   1 2 3 4 5 6 ->
     *   0 0 0 0 0 0
     */
    uint32_t renumbered[SAVED_REGISTER_MAX];
    for (int i = 0; i < count; ++i) {
        unsigned countless = 0;
        for (int j = 0; j < i; ++j)
            if (registers[j] < registers[i])
                countless++;

        renumbered[i] = registers[i] - countless - 1;
    }
    
    uint32_t permutation = 0;

    /*
     * Using the renumbered list, we map each element of the list (positionally) into a range large enough to represent
     * the range of any valid element, as well as be subdivided to represent the range of later elements.
     *
     * For example, if we use a factor of 120 for the first position (encoding multiples, decoding divides), that
     * provides us with a range of 0-719. There are 6 possible values that may be encoded in 0-719 (assuming later
     * division by 120), the range is broken down as:
     *
     *   0   - 119: 0
     *   120 - 239: 1
     *   240 - 359: 2
     *   360 - 479: 3
     *   480 - 599: 4
     *   600 - 719: 5
     *
     * Within that range, further positions may be encoded. Assuming a value of 1 in position 0, and a factor of
     * 24 for position 1, the range breakdown would be as follows:
     *   120 - 143: 0
     *   144 - 167: 1
     *   168 - 191: 2
     *   192 - 215: 3
     *   216 - 239: 4
     *
     * Note that due to the positional renumbering performed prior to this step, we know that each subsequent position
     * in the list requires fewer elements; eg, position 0 may include 0-5, position 1 0-4, and position 2 0-3. This
     * allows us to allocate smaller overall ranges to represent all possible elements.
     */
    PLCF_ASSERT(SAVED_REGISTER_MAX == 6);
    switch (count) {
        case 1:
            permutation |= renumbered[0];
            break;

        case 2:
            permutation |= (5*renumbered[0] + renumbered[1]);
            break;

        case 3:
            permutation |= (20*renumbered[0] + 4*renumbered[1] + renumbered[2]);
            break;
            
        case 4:
            permutation |= (60*renumbered[0] + 12*renumbered[1] + 3*renumbered[2] + renumbered[3]);
            break;
            
        case 5:
            permutation |= (120*renumbered[0] + 24*renumbered[1] + 6*renumbered[2] + 2*renumbered[3] + renumbered[4]);
            break;
            
        case 6:
            /*
             * There are 6 elements in the list, 6 possible values for each element, and values may not repeat. The
             * value of the last element can be derived from the values previously seen (and due to the positional
             * renumbering performed above, the value of the last element will *always* be 0.
             */
            permutation |= (120*renumbered[0] + 24*renumbered[1] + 6*renumbered[2] + 2*renumbered[3] + renumbered[4]);
            break;
    }
    
    PLCF_ASSERT((permutation & 0x3FF) == permutation);
    return permutation;
}

/**
 * Decode a 10 bit ordered register list.
 */
static void reg_permute_decode (uint32_t permutation, uint32_t registers[SAVED_REGISTER_MAX], uint32_t count) {
    PLCF_ASSERT(count <= SAVED_REGISTER_MAX);

    /*
     * Each register is encoded by mapping the values to a 10-bit range, and then further sub-ranges within that range,
     * with a subrange allocated to each position. See the encoding function for full documentation.
     */
	int permunreg[SAVED_REGISTER_MAX];
#define PERMUTE(pos, factor) do { \
    permunreg[pos] = permutation/factor; \
    permutation -= (permunreg[pos]*factor); \
} while (0)

    PLCF_ASSERT(SAVED_REGISTER_MAX == 6);
	switch (count) {
		case 6:
            PERMUTE(0, 120);
            PERMUTE(1, 24);
            PERMUTE(2, 6);
            PERMUTE(3, 2);
            PERMUTE(4, 1);
            
            /*
             * There are 6 elements in the list, 6 possible values for each element, and values may not repeat. The
             * value of the last element can be derived from the values previously seen (and due to the positional
             * renumbering performed, the value of the last element will *always* be 0).
             */
            permunreg[5] = 0;
			break;
		case 5:
            PERMUTE(0, 120);
            PERMUTE(1, 24);
            PERMUTE(2, 6);
            PERMUTE(3, 2);
            PERMUTE(4, 1);
			break;
		case 4:
            PERMUTE(0, 60);
            PERMUTE(1, 12);
            PERMUTE(2, 3);
            PERMUTE(3, 1);
			break;
		case 3:
            PERMUTE(0, 20);
            PERMUTE(1, 4);
            PERMUTE(2, 1);
			break;
		case 2:
            PERMUTE(0, 5);
            PERMUTE(1, 1);
			break;
		case 1:
            PERMUTE(0, 1);
			permunreg[0] = permutation;
			break;
	}
#undef PERMUTE

	/* Recompute the actual register values based on the position-relative values. */
	bool position_used[SAVED_REGISTER_MAX+1] = { 0 };

	for (uint32_t i = 0; i < count; ++i) {
		int renumbered = 0;
		for (int u = 1; u < 7; u++) {
			if (!position_used[u]) {
				if (renumbered == permunreg[i]) {
					registers[i] = u;
					position_used[u] = true;
					break;
				}
				renumbered++;
			}
		}
	}
}

/**
 * Decode an x86 frameless encoding.
 */
- (void) testX86DecodeFrameless {
    // TODO
#if 0
    /* Create a frame encoding, with registers saved at ebp-1020 bytes */
    const uint32_t encoded_stack_size = 1020;
    const uint32_t encoded_regs[SAVED_REGISTER_MAX] = {
        UNWIND_X86_REG_ESI,
        UNWIND_X86_REG_EDX,
        UNWIND_X86_REG_ECX,
    };
    const uint32_t encoded_regs_count = 3;
    const uint32_t encoded_reg_permutation = reg_permute_encode(encoded_regs, encoded_regs_count);
    
    uint32_t encoding = UNWIND_X86_MODE_STACK_IMMD |
        INSERT_BITS(encoded_stack_size/4, UNWIND_X86_FRAMELESS_STACK_SIZE) |
        INSERT_BITS(encoded_regs_count, UNWIND_X86_FRAMELESS_STACK_REG_COUNT) |
        INSERT_BITS(encoded_reg_permutation, UNWIND_X86_FRAMELESS_STACK_REG_PERMUTATION);

    /* Try extracting it */
    uint32_t stack_size = EXTRACT_BITS(encoding, UNWIND_X86_FRAMELESS_STACK_SIZE) * 4;
    uint32_t reg_count = EXTRACT_BITS(encoding, UNWIND_X86_FRAMELESS_STACK_REG_COUNT);
    uint32_t reg_permutation = EXTRACT_BITS(encoding, UNWIND_X86_FRAMELESS_STACK_REG_PERMUTATION);

    STAssertEquals(stack_size, encoded_stack_size, @"Incorrect stack size decoded");
    STAssertEquals(reg_count, encoded_regs_count, @"Incorrect register count decoded");
    STAssertEquals(reg_permutation, encoded_reg_permutation, @"Incorrect register permutation decoded");

    /* Extract the registers. */
    uint32_t regs[encoded_regs_count];
    reg_permute_decode(reg_permutation, regs, reg_count);
    for (uint32_t i = 0; i < reg_count; i++) {
        fprintf(stderr, "got %d want %d\n", regs[i], encoded_regs[i]);
    }
    
    
    for (uint32_t i = 0; i < reg_count; i++) {
        STAssertEquals(regs[i], encoded_regs[i], @"Incorrect register value extracted for position %" PRId32, i);
    }
#endif
}

/**
 * Decode an x86-64 frameless encoding
 */
- (void) testX86_64_DecodeFrameless_Self {
    // an example encoded by clang
    // stack size=40, rbx,r12,r14,r15
    const uint32_t encoded_regs_count = 4;
    const uint32_t encoded_stack_size = 40;
    const uint32_t encoded_regs[SAVED_REGISTER_MAX] = {
        UNWIND_X86_64_REG_RBX,
        UNWIND_X86_64_REG_R12,
        UNWIND_X86_64_REG_R14,
        UNWIND_X86_64_REG_R15
    };
    const uint32_t encoded_regs_permutation = reg_permute_encode(encoded_regs, encoded_regs_count);
    uint32_t encoded = UNWIND_X86_MODE_STACK_IMMD |
        INSERT_BITS(encoded_stack_size/8, UNWIND_X86_FRAMELESS_STACK_SIZE) |
        INSERT_BITS(encoded_regs_count, UNWIND_X86_FRAMELESS_STACK_REG_COUNT) |
        INSERT_BITS(encoded_regs_permutation, UNWIND_X86_FRAMELESS_STACK_REG_PERMUTATION);

    /* Extract the entry fields */
    const uint32_t stack_size = EXTRACT_BITS(encoded, UNWIND_X86_64_FRAMELESS_STACK_SIZE) * 8;
    const uint32_t regs_count = EXTRACT_BITS(encoded, UNWIND_X86_64_FRAMELESS_STACK_REG_COUNT);
    const uint32_t regs_permutation = EXTRACT_BITS(encoded, UNWIND_X86_64_FRAMELESS_STACK_REG_PERMUTATION);
    
    STAssertEquals(stack_size, encoded_stack_size, @"Incorrect stack size decoded");
    STAssertEquals(regs_count, encoded_regs_count, @"Incorrect register count decoded");
    STAssertEquals(regs_permutation, encoded_regs_permutation, @"Incorrect register permutation decoded");
    
    /* Extract and verify the registers */
    uint32_t regs[regs_count];
    reg_permute_decode(regs_permutation, regs, regs_count);
    for (uint32_t i = 0; i < regs_count; i++) {
        STAssertEquals(regs[i], encoded_regs[i], @"Incorrect register value extracted for position %" PRId32, i);
    }
}

/**
 * Decode an x86-64 frameless encoding
 */
- (void) testX86_64_DecodeFrameless {
    // an example encoded by clang
    // stack size=40, rbx,r12,r14,r15
    const uint32_t encoded = 0x02051004;
    const uint32_t encoded_regs_count = 4;
    const uint32_t encoded_stack_size = 40;
    const uint32_t encoded_regs_permutation = EXTRACT_BITS(encoded, UNWIND_X86_64_FRAMELESS_STACK_REG_PERMUTATION);
    const uint32_t encoded_regs[SAVED_REGISTER_MAX] = {
        UNWIND_X86_64_REG_RBX,
        UNWIND_X86_64_REG_R12,
        UNWIND_X86_64_REG_R14,
        UNWIND_X86_64_REG_R15
    };

    /* Extract the entry fields */
    const uint32_t stack_size = EXTRACT_BITS(encoded, UNWIND_X86_64_FRAMELESS_STACK_SIZE) * 8;
    const uint32_t regs_count = EXTRACT_BITS(encoded, UNWIND_X86_64_FRAMELESS_STACK_REG_COUNT);
    const uint32_t regs_permutation = EXTRACT_BITS(encoded, UNWIND_X86_64_FRAMELESS_STACK_REG_PERMUTATION);
    
    STAssertEquals(stack_size, encoded_stack_size, @"Incorrect stack size decoded");
    STAssertEquals(regs_count, encoded_regs_count, @"Incorrect register count decoded");
    STAssertEquals(regs_permutation, encoded_regs_permutation, @"Incorrect register permutation decoded");

    /* Extract and verify the registers */
    uint32_t regs[regs_count];
    reg_permute_decode(regs_permutation, regs, regs_count);
    for (uint32_t i = 0; i < regs_count; i++) {
        STAssertEquals(regs[i], encoded_regs[i], @"Incorrect register value extracted for position %" PRId32, i);
    }
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
