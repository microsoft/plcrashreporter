/*
 * Author: Landon Fuller <landonf@plausible.coop>
 *
 * Copyright (c) 2012-2013 Plausible Labs Cooperative, Inc.
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

@implementation PLCrashTestCase

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

@end
