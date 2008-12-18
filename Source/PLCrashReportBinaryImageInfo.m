/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008 Plausible Labs Cooperative, Inc.
 * All rights reserved.
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
