/*
 * Author: Landon Fuller <landonf@plausible.coop>
 *
 * Copyright (c) 2015 Plausible Labs Cooperative, Inc.
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

#import "SenTestCompat.h"
#import "PLCrashDLCompat.h"

#import <objc/runtime.h>

@interface PLCrashDLCompatTests : SenTestCase @end

@implementation PLCrashDLCompatTests

- (void) testDladdr {
    Dl_info dli;
    
    /* Fetch the our IMP address. */
    IMP localIMP = class_getMethodImplementation([self class], _cmd);
    
    /* Try to look it up using our dladdr replacement */
    STAssertNotEquals(pl_dladdr((void *)localIMP, &dli), 0, @"Failed to look up symbol");
    
    /* Sanity check the result */
    STAssertNotNULL(dli.dli_fname, @"Image name not found");
    STAssertNotNULL(dli.dli_fbase, @"Image base not found");
    
    STAssertNotNULL(dli.dli_sname, @"Symbol name not found");
    STAssertNotNULL(dli.dli_saddr, @"Symbol address not found");
    
    STAssertEqualCStrings(dli.dli_sname, "-[PLCrashDLCompatTests testDladdr]", @"Incorrect symbol name");
    STAssertEquals((void *)localIMP, dli.dli_saddr, @"Incorrect symbol address");
}

@end
