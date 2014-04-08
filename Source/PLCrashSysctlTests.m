/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008-2013 Plausible Labs Cooperative, Inc.
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

/* Test handling of truncated UTF-8 strings */
- (void) testValidUTF8Strlen {
    /* Test handling (and interaction) of maxlen and NUL termination */
    {
        STAssertEquals(plcrash_sysctl_valid_utf8_bytes_max((uint8_t[]) {'a', '\0'} , 100), (size_t)1,
                       @"String iteration ignored NUL terminator in favor of maxlen");

        STAssertEquals(plcrash_sysctl_valid_utf8_bytes_max((uint8_t[]) {'a', 'a', '\0'}, 1), (size_t)1,
                       @"String iteration ignored maxlen in favor of NUL");
    }
    
    /* Test handling (and interaction) of maxlen and multibyte validation */
    {
        /* This is a valid multibyte sequences; we use maxlen to terminate in the middle of it */
        STAssertEquals(plcrash_sysctl_valid_utf8_bytes_max((uint8_t[]) {'a', 0xC2, 0x80, '\0'}, 2), (size_t)1,
                       @"Multibyte validation ignored maxlen");
        
        /* Verify that maxlen doesn't trigger *early* termination. This also sanity-checks the above test,
         * asserting that had maxlen not been set too low, the characters would have been correctly validated */
        STAssertEquals(plcrash_sysctl_valid_utf8_bytes_max((uint8_t[]) {'a', 0xC2, 0x80, '\0'}, 3), (size_t)3,
                       @"Maxlen value triggered incorrect early termination of multibyte validation");
    }

#define LEN_UTF8(_c, ...) \
    plcrash_sysctl_valid_utf8_bytes((uint8_t[]) {_c, ##__VA_ARGS__})

    /* Test BOM handling. BOM is not useful or recommended for UTF-8 encoding, but it's still necessary to support. */
    {
        STAssertEquals(LEN_UTF8(0xEF, 0xBB, 0xBF, 'a', '\0'), (size_t)4, @"0 continutation rejected in-range byte");
    }

    /* Test 0 continuation sequence */
    {
        STAssertEquals(LEN_UTF8('a', 127, '\0'), (size_t)2, @"Rejected in-range byte");
        STAssertEquals(LEN_UTF8('a', 128, '\0'), (size_t)1, @"Accepted out-of-range byte");
    }

    /* Test 1 byte continuation of 2 byte sequence */
    {
        /* Verify that bytes that fall within the expected range are accepted */
        STAssertEquals(LEN_UTF8('a', 0xC2, 0x80, '\0'), (size_t)3, @"Rejected in-range byte (128)");
        STAssertEquals(LEN_UTF8('a', 0xDF, 0xBF, '\0'), (size_t)3, @"Rejected in-range byte (2047)");

        /* Verify that a missing byte in a 2 byte sequence is considered an error */
        STAssertEquals(LEN_UTF8('a', 0xC0, '\0'), (size_t)1, @"Accepted leading byte with missing continuation");

        /* Verify that an invalid 2nd byte in a 2 byte sequence is considered an error */
        STAssertEquals(LEN_UTF8('a', 0xC0, 0x00, '\0'), (size_t)1, @"Accepted leading byte with invalid continuation");
    }
    
    /* Test 2 byte continuation of 3 byte sequence */
    {
        /* Verify that bytes that fall within the expected range are accepted */
        STAssertEquals(LEN_UTF8('a', 0xE0, 0xA0, 0x80, '\0'), (size_t)4, @"Rejected in-range byte (2048)");
        STAssertEquals(LEN_UTF8('a', 0xEF, 0xBF, 0xBF, '\0'), (size_t)4, @"Rejected in-range byte (65535)");
        
        /* Verify that missing trailing bytes in a 3 byte sequence are considered an error */
        STAssertEquals(LEN_UTF8('a', 0xE0, '\0'), (size_t)1, @"Accepted leading byte with missing continuations");
        STAssertEquals(LEN_UTF8('a', 0xE0, 0x80, '\0'), (size_t)1, @"Accepted leading byte with missing continuations");
        STAssertEquals(LEN_UTF8('a', 0xE0, 0x80, 0x80, '\0'), (size_t)4, @"Rejected sequence containing full byte allotment");

        /* Verify that invalid trailing bytes in a 3 byte sequence are considered an error */
        STAssertEquals(LEN_UTF8('a', 0xE0, 0x00, '\0'), (size_t)1, @"Accepted leading byte with invalid continuation");
        STAssertEquals(LEN_UTF8('a', 0xE0, 0x80, 0x0, '\0'), (size_t)1, @"Accepted leading bytes with invalid continuation");
    }
    
    /* Test 3 byte continuation of 4 byte sequence */
    {
        /* Verify that bytes that fall within the expected range are accepted */
        STAssertEquals(LEN_UTF8('a', 0xF0, 0x90, 0x80, 0x80, '\0'), (size_t)5, @"Rejected in-range byte (65536)");
        STAssertEquals(LEN_UTF8('a', 0xF4, 0x8F, 0xBF, 0xBF, '\0'), (size_t)5, @"Rejected in-range byte (1114111)");
        
        /* Verify that missing trailing bytes in a 4 byte sequence are considered an error */
        STAssertEquals(LEN_UTF8('a', 0xF0, '\0'), (size_t)1, @"Accepted leading byte with missing continuations");
        STAssertEquals(LEN_UTF8('a', 0xF0, 0x80, '\0'), (size_t)1, @"Accepted leading byte with missing continuations");
        STAssertEquals(LEN_UTF8('a', 0xF0, 0x80, 0x80, 0x80, '\0'), (size_t)5, @"Rejected sequence containing full byte allotment");
        
        /* Verify that invalid trailing bytes in a 4 byte sequence are considered an error */
        STAssertEquals(LEN_UTF8('a', 0xF0, 0x00, '\0'), (size_t)1, @"Accepted leading byte with invalid continuation");
        STAssertEquals(LEN_UTF8('a', 0xF0, 0x80, 0x0, '\0'), (size_t)1, @"Accepted leading bytes with invalid continuation");
        STAssertEquals(LEN_UTF8('a', 0xF0, 0x80, 0x80, 0x0, '\0'), (size_t)1, @"Accepted leading bytes with invalid continuation");

    }
    
    /* Verify that the implementation correctly resets its internal state after fully parsing a UTF-8 code
     * point; this is just a test of its behavior with a multiple codepoint string */
    {
        STAssertEquals(LEN_UTF8('a', 0xC2, 0x80, 0xC2, 0x80, '\0'), (size_t)5, @"Rejected valid UTF-8 string");
        
        /* This should return the length of the valid bytes, ignoring the invalid trailing multibyte sequence */
        STAssertEquals(LEN_UTF8('a', 0xC2, 0x80, /* Invalid */ 0xC2, '\0'), (size_t)3, @"Lost valid UTF-8 prefix when rejecting trailing bytes");
    }

#undef LEN_UTF8
}

@end
