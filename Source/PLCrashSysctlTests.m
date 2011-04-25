/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008-2011 Plausible Labs Cooperative, Inc.
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

#import "PLCrashSysctl.h"

@interface PLCrashSysctlTests : SenTestCase @end

@implementation PLCrashSysctlTests

/* Test fetch of a string value */
- (void) testSysctlString {
    char *string = plcrash_sysctl_string("kern.ostype");
    STAssertNotNULL(string, @"Failed to fetch string value");

    // This is a bit fragile 
    STAssertEqualCStrings(string, "Darwin", @"Did not fetch expected OS type");

    free(string);
}

/* Test fetch of an integer value */
- (void) testSysctlInteger {
    int result;

    STAssertTrue(plcrash_sysctl_int("hw.logicalcpu_max", &result), @"Failed to fetch int value");
    STAssertEquals(result, (int)[[NSProcessInfo processInfo] processorCount], @"Incorrect count");
}


@end
