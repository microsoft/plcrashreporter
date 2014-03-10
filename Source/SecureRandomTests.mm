/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008-2014 Plausible Labs Cooperative, Inc.
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

#import "SecureRandom.hpp"
#include <sys/stat.h>
#include <math.h>

using namespace plcrash::async;

@interface PLCrashAsyncSecureRandomTests : SenTestCase @end

/**
 * Tests the SecureRandom implementation.
 */
@implementation PLCrashAsyncSecureRandomTests

- (void) testReadBytes {
    SecureRandom rnd = SecureRandom();
    uint8_t orig[1024];
    uint8_t bytes[1024];
    
    /* Populate test buffers. The first, orig, holds test data we expect to be overwritten.
     * The second, bytes, is the buffer that *should* be overwritten with random data */
    memset(orig, 'A', sizeof(orig));
    memcpy(bytes, orig, sizeof(bytes));

    STAssertEquals(PLCRASH_ESUCCESS, rnd.readBytes(bytes, sizeof(bytes)), @"Read failed");
    
    /* A simple sanity check to verify that *something* happens */
    STAssertTrue(memcmp(orig, bytes, sizeof(orig)) != 0, @"Buffer unmodified");
    
    /*
     * Measure the Shannon entropy of the result. This is a sanity check to make sure we're actually
     * fetching random data. It's not intended to actually provide any concrete tests or guarantees
     * regarding the entropy source.
     *
     * This entropy measurement code was written by Troy D. Hanson and released to the public domain:
     * http://troydhanson.github.io/misc/Entropy.html
     */
    unsigned int counts[256] = { 0 };
    unsigned int total;

    /* Accumulate counts of each byte value */
    for (size_t i = 0; i < sizeof(bytes); i++) {
        uint8_t byte = bytes[i];
        counts[byte]++;
        total++;
    }

    /*
     * Compute the final entropy. this is -SUM[0,255](p*l(p)) where p is the probability of byte value [0..255]
     * and l(p) is the base-2 log of p. Unit is bits per byte.
     */
    double sum = 0;
    for (int i = 0; i < 256; i++) {
        if (counts[i] == 0)
            continue;
        
        double p = 1.0 * counts[i] / total;
        double lp = log(p) / log(2);
        sum -= p*lp;
    }

    /* Verify a reasonable level of entropy. We're not trying to validate the underlying randomness source, we're just
     * trying to verify that we do in fact populate the buffer with something random looking. */
    STAssertGreaterThan(sum, 0.3, @"Expected more entropy than %.2g from the RNG", sum);
}

- (void) testUniform {
    SecureRandom rnd = SecureRandom();
    
    /* Initialize to value outside the range, so that we can verify that the result is actually set. */
    uint32_t result = 42;

    /* Generate a uniformally random number. */
    STAssertEquals(PLCRASH_ESUCCESS, rnd.uniform(20, &result), @"Failed to fetch uniform value");
    STAssertTrue(result < 20, @"Returned result outside of the expected range");
}

@end
