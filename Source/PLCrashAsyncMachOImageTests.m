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

@interface PLCrashAsyncMachOImageTests : SenTestCase {
    plcrash_async_macho_image_t _image;
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

    plcrash_async_macho_image_init(&_image, mach_task_self(), info.dli_fname, (pl_vm_address_t) info.dli_fbase, vmaddr_slide);

    /* Basic test of the initializer */
    STAssertEqualCStrings(_image.name, info.dli_fname, @"Incorrect name");
    STAssertEquals(_image.header_addr, (pl_vm_address_t) info.dli_fbase, @"Incorrect header address");
    STAssertEquals(_image.vmaddr_slide, (int64_t) vmaddr_slide, @"Incorrect vmaddr_slide value");
}

- (void) tearDown {
    plcrash_async_macho_image_free(&_image);
}

/**
 * Test iteration of Mach-O load commands.
 */
- (void) testIterateCommand {
    pl_vm_address_t cmd_addr = 0;

    bool found_uuid = false;
    while ((cmd_addr = plcrash_async_macho_image_next_command(&_image, cmd_addr)) != 0) {
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

@end