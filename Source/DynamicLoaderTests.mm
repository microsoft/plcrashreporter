/*
 * Author: Landon Fuller <landonf@plausible.coop>
 *
 * Copyright (c) 2013 - 2015 Plausible Labs Cooperative, Inc.
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


#include "DynamicLoader.hpp"

using namespace plcrash::async;

@interface PLCrashAsyncDynamicLoaderTests : SenTestCase

@end

@implementation PLCrashAsyncDynamicLoaderTests

- (void) testFindDynamicLoaderInfo {
    DynamicLoader dyld;
    
    STAssertEquals(DynamicLoader::nasync_get(mach_task_self(), dyld), PLCRASH_ESUCCESS, @"Failed to fetch dyld info");

    /* If this is an LP64 binary, so should be our dyld image data (and vis versa) */
#ifdef __LP64__
    integer_t expectedFormat = TASK_DYLD_ALL_IMAGE_INFO_64;
#else
    integer_t expectedFormat = TASK_DYLD_ALL_IMAGE_INFO_32;
#endif
    
    STAssertEquals(dyld.dyld_info().all_image_info_format, expectedFormat, @"Got unexpected image info format");
}

@end
