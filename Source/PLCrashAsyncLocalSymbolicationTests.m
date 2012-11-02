/*
 * Author: Mike Ash <mikeash@plausiblelabs.com>
 *
 * Copyright (c) 2012 Plausible Labs Cooperative, Inc.
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

#import <dlfcn.h>
#import <mach-o/dyld.h>
#import <mach-o/getsect.h>

#import "PLCrashAsyncMachOImage.h"
#import "PLCrashAsyncLocalSymbolication.h"


@interface PLCrashAsyncLocalSymbolicationTests : SenTestCase {
    /** The image containing our class. */
    pl_async_macho_t _image;
}

@end

@implementation PLCrashAsyncLocalSymbolicationTests

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
    
    unsigned long text_size;
    STAssertNotNULL(getsegmentdata(info.dli_fbase, SEG_TEXT, &text_size), @"Failed to find segment");
    STAssertEquals(_image.text_size, (pl_vm_size_t) text_size, @"Incorrect text segment size computed");
}

- (void) tearDown {
    pl_async_macho_free(&_image);
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

void PLCrashAsyncLocalSymbolicationTestsDummyFunction(void) {}

- (void) testFindSymbol {
    struct testFindSymbol_cb_ctx ctx = {};
    plcrash_error_t err;
    
    err = pl_async_local_find_symbol(&_image, (pl_vm_address_t)PLCrashAsyncLocalSymbolicationTestsDummyFunction, testFindSymbol_cb, &ctx);
    STAssertEquals(err, PLCRASH_ESUCCESS, @"Got error trying to find symbol");
    STAssertEquals(ctx.addr, (pl_vm_address_t)PLCrashAsyncLocalSymbolicationTestsDummyFunction, @"Got bad address finding symbol");
    STAssertEqualCStrings(ctx.name, "_PLCrashAsyncLocalSymbolicationTestsDummyFunction", @"Got wrong symbol name");
    
    pl_vm_address_t localPC = [[[NSThread callStackReturnAddresses] objectAtIndex: 0] longLongValue];
    err = pl_async_local_find_symbol(&_image, localPC, testFindSymbol_cb, &ctx);
    STAssertEquals(err, PLCRASH_ESUCCESS, @"Got error trying to find symbol");
    STAssertEquals(ctx.addr, (pl_vm_address_t)[self methodForSelector: _cmd], @"Got bad address finding symbol");
    STAssertEqualCStrings(ctx.name, "-[PLCrashAsyncLocalSymbolicationTests testFindSymbol]", @"Got wrong symbol name");
}

@end
