/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008-2012 Plausible Labs Cooperative, Inc.
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

#import "PLCrashAsyncMachOImage.h"

#import <dlfcn.h>
#import <mach-o/dyld.h>
#import <mach-o/getsect.h>
#import <objc/runtime.h>
#import <execinfo.h>

@interface PLCrashAsyncMachOImageTests : SenTestCase {
    /** The image containing our class. */
    pl_async_macho_t _image;
}
@end


@implementation PLCrashAsyncMachOImageTests

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
}

- (void) tearDown {
    pl_async_macho_free(&_image);
}

/**
 * Test iteration of Mach-O load commands.
 */
- (void) testIterateCommand {
    pl_vm_address_t cmd_addr = 0;

    bool found_uuid = false;
    while ((cmd_addr = pl_async_macho_next_command(&_image, cmd_addr)) != 0) {
        /* Read the load command */
        struct uuid_command cmd;

        /* First, read just the standard load_command data. */
        STAssertEquals(plcrash_async_read_addr(_image.task, cmd_addr, &cmd, sizeof(struct load_command)), KERN_SUCCESS, @"Read failed");
        
        /* If this is not LC_UUID command, nothing to do */
        if (_image.swap32(cmd.cmd) != LC_UUID)
            continue;
        
        /* Read in the full UUID command */
        STAssertEquals(plcrash_async_read_addr(_image.task, cmd_addr, &cmd, sizeof(cmd)), KERN_SUCCESS, @"Read failed");
        
        // TODO - Validate the UUID value?
        STAssertFalse(found_uuid, @"Duplicate LC_UUID load commands iterated");
        found_uuid = true;
    }

    STAssertTrue(found_uuid, @"Failed to iterate LC_CMD structures");
}

/**
 * Test type-specific iteration of Mach-O load commands.
 */
- (void) testIterateSpecificCommand {
    pl_vm_address_t cmd_addr = 0;
    
    bool found_uuid = false;
    uint32_t cmdsize = 0;

    while ((cmd_addr = pl_async_macho_next_command_type(&_image, cmd_addr, LC_UUID, &cmdsize)) != 0) {
        /* Read the load command */
        struct uuid_command cmd;
                
        /* Read in the full UUID command */
        STAssertEquals(plcrash_async_read_addr(_image.task, cmd_addr, &cmd, sizeof(cmd)), KERN_SUCCESS, @"Read failed");
        
        /* Validate the command type and size */
        STAssertEquals(_image.swap32(cmd.cmd), (uint32_t)LC_UUID, @"Incorrect load command returned");
        STAssertEquals(_image.swap32(cmd.cmdsize), cmdsize, @"Incorrect load command size returned by iterator");

        STAssertFalse(found_uuid, @"Duplicate LC_UUID load commands iterated");
        found_uuid = true;
    }

    STAssertTrue(found_uuid, @"Failed to iterate LC_CMD structures");
    
    /* Test the case where there are no matches. LC_SUB_UMBRELLA should never be used in a unit tests binary. */
    cmd_addr = pl_async_macho_next_command_type(&_image, 0, LC_SUB_UMBRELLA, NULL);
    STAssertEquals((pl_vm_address_t)0, cmd_addr, @"Should not have found the requested load command");
}

/**
 * Test type-specific iteration of Mach-O load commands when a NULL size argument is provided.
 */
- (void) testIterateSpecificCommandNULLSize {
    pl_vm_address_t cmd_addr = 0;
    
    /* If the following doesn't crash dereferencing the NULL cmdsize argument, success! */
    bool found_uuid = false;
    while ((cmd_addr = pl_async_macho_next_command_type(&_image, cmd_addr, LC_UUID, NULL)) != 0) {
        STAssertFalse(found_uuid, @"Duplicate LC_UUID load commands iterated");
        found_uuid = true;
    }
    
    STAssertTrue(found_uuid, @"Failed to iterate LC_CMD structures");
}

/**
 * Test simple short-cut for finding a single load_command.
 */
- (void) testFindCommand {
    struct uuid_command cmd;
    pl_vm_address_t cmd_addr = pl_async_macho_find_command(&_image, LC_UUID, &cmd, sizeof(cmd));
    STAssertNotEquals((pl_vm_address_t)0, cmd_addr, @"Failed to find command");
    STAssertEquals(_image.swap32(cmd.cmd), (uint32_t)LC_UUID, @"Incorrect load command returned");
    STAssertEquals(_image.swap32(cmd.cmdsize), (uint32_t)sizeof(cmd), @"Incorrect load command size returned");
    
    /* Test the case where there are no matches. LC_SUB_UMBRELLA should never be used in a unit tests binary. */
    cmd_addr = pl_async_macho_find_command(&_image, LC_SUB_UMBRELLA, NULL, 0);
    STAssertEquals((pl_vm_address_t)0, cmd_addr, @"Should not have found the requested load command");
}

/**
 * Test memory mapping of a Mach-O segment
 */
- (void) testMapSegment {
    plcrash_async_mobject_t mobj;

    /* Try to map the segment */
    STAssertEquals(PLCRASH_ESUCCESS, pl_async_macho_map_segment(&_image, "__LINKEDIT", &mobj), @"Failed to map segment");
    
    /* Fetch the segment directly for comparison */
    unsigned long segsize = 0;
    uint8_t *data = getsegmentdata((void *)_image.header_addr, "__LINKEDIT", &segsize);
    STAssertNotNULL(data, @"Could not fetch segment data");

    /* Compare the address and length. We have to apply the slide to determine the original source address. */    
    STAssertEquals((pl_vm_address_t)data, (pl_vm_address_t) (mobj.address + mobj.vm_slide), @"Addresses do not match");
    STAssertEquals((pl_vm_size_t)segsize, mobj.length, @"Sizes do not match");

    /* Compare the contents */
    uint8_t *mapped_data = plcrash_async_mobject_pointer(&mobj, mobj.address, segsize);
    STAssertNotNULL(mapped_data, @"Could not get pointer for mapped data");

    STAssertNotEquals(mapped_data, data, @"Should not be the same pointer!");
    STAssertTrue(memcmp(data, mapped_data, segsize) == 0, @"The mapped data is not equal");

    /* Clean up */
    plcrash_async_mobject_free(&mobj);

    /* Test handling of a missing segment */
    STAssertEquals(PLCRASH_ENOTFOUND, pl_async_macho_map_segment(&_image, "__NO_SUCH_SEG", &mobj), @"Should have failed to map the segment");
}


/**
 * Test memory mapping of a missing Mach-O segment
 */
- (void) testMapMissingSegment {
    plcrash_async_mobject_t mobj;
    STAssertEquals(PLCRASH_ENOTFOUND, pl_async_macho_map_segment(&_image, "__NO_SUCH_SEG", &mobj), @"Should have failed to map the segment");
}

/* testFindSymbol callback handling */

struct testFindSymbol_cb_ctx {
    pl_vm_address_t addr;
    char *name;
};

static void testFindSymbol_cb (pl_vm_address_t address, const char *name, void *ctx) {
    struct testFindSymbol_cb_ctx *cb_ctx = ctx;
    cb_ctx->addr = address;
    cb_ctx->name = strdup(name);
}

/**
 * Test symbol lookup.
 */
- (void) testFindSymbol {
    /* Fetch our current PC, to be used for symbol lookup */
    void *callstack[1];
    int frames = backtrace(callstack, 1);
    STAssertEquals(1, frames, @"Could not fetch our PC");

    /* Perform our symbol lookup */
    struct testFindSymbol_cb_ctx ctx;
    plcrash_error_t res = pl_async_macho_find_symbol(&_image, (pl_vm_address_t) callstack[0], testFindSymbol_cb, &ctx);
    STAssertEquals(res, PLCRASH_ESUCCESS, @"Failed to locate symbol");
    
    /* The following tests will crash if the above did not succeed */
    if (res != PLCRASH_ESUCCESS)
        return;
    
    /* Fetch the our IMP address and symbolicate it using dladdr(). */
    IMP localIMP = class_getMethodImplementation([self class], _cmd);
    Dl_info dli;
    STAssertTrue(dladdr((void *)localIMP, &dli) != 0, @"Failed to look up symbol");

    /* Compare the results */
    STAssertEqualCStrings(dli.dli_sname, ctx.name, @"Returned incorrect symbol name");
    STAssertEquals(dli.dli_saddr, (void *) ctx.addr, @"Returned incorrect symbol address with slide %" PRId64, _image.vmaddr_slide);
}

@end