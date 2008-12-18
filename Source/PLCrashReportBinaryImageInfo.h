/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008 Plausible Labs Cooperative, Inc.
 * All rights reserved.
 */

#import <Foundation/Foundation.h>

@interface PLCrashReportBinaryImageInfo : NSObject {
@private
    /** Base image address */
    uint64_t _baseAddress;

    /** Image segment size */
    uint64_t _imageSize;

    /** Name of binary image */
    NSString *_imageName;

    /** If the UUID is available */
    BOOL _hasImageUUID;

    /** 128-bit object UUID. May be nil. */
    NSString *_imageUUID;
}

- (id) initWithImageBaseAddress: (uint64_t) baseAddress 
                      imageSize: (uint64_t) imageSize
                      imageName: (NSString *) imageName
                      imageUUID: (NSString *) imageUUID;

/**
 * Image base address.
 */
@property(nonatomic, readonly) uint64_t imageBaseAddress;

/**
 * Segment size.
 */
@property(nonatomic, readonly) uint64_t imageSize;

/**
 * Image name (absolute path)
 */
@property(nonatomic, readonly) NSString *imageName;


/**
 * YES if this image has an associated UUID.
 */
@property(nonatomic, readonly) BOOL hasImageUUID;

/**
 * 128-bit object UUID (matches Mach-O DWARF dSYM files). May be nil if unavailable.
 */
@property(nonatomic, readonly) NSString *imageUUID;

@end
