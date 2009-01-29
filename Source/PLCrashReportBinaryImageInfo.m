/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008-2009 Plausible Labs Cooperative, Inc.
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

#import "PLCrashReportBinaryImageInfo.h"

/**
 * Crash Log binary image info. Represents an executable or shared library.
 */
@implementation PLCrashReportBinaryImageInfo

/**
 * Initialize with the given binary image properties.
 */
- (id) initWithImageBaseAddress: (uint64_t) baseAddress 
                      imageSize: (uint64_t) imageSize
                      imageName: (NSString *) imageName
                      imageUUID: (NSString *) imageUUID
{
    if ((self = [super init]) == nil)
        return nil;
    
    _baseAddress = baseAddress;
    _imageSize = imageSize;
    _imageName = [imageName retain];
    _imageUUID = [imageUUID retain];
    if (_imageUUID != nil)
        _hasImageUUID = YES;

    return self;
}

- (void) dealloc {
    [_imageName release];
    [_imageUUID release];
    [super dealloc];
}

@synthesize imageBaseAddress = _baseAddress;
@synthesize imageSize = _imageSize;
@synthesize imageName = _imageName;
@synthesize hasImageUUID = _hasImageUUID;
@synthesize imageUUID = _imageUUID;

@end
