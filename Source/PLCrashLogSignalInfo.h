/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008 Plausible Labs Cooperative, Inc.
 * All rights reserved.
 */

#import <Foundation/Foundation.h>

@interface PLCrashLogSignalInfo : NSObject {
@private
    /** Signal name */
    NSString *_name;
    
    /** Signal code */
    NSString *_code;

    /** Fauling instruction or address */
    uint64_t _address;
}

- (id) initWithSignalName: (NSString *) name code: (NSString *) code address: (uint64_t) address;

/**
 * The signal name.
 */
@property(nonatomic, readonly) NSString *name;

/**
 * The signal code.
 */
@property(nonatomic, readonly) NSString *code;

/**
 * The faulting instruction or address.
 */
@property(nonatomic, readonly) uint64_t address;

@end
