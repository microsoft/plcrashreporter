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
}

- (id) initWithSignalName: (NSString *) name code: (NSString *) code;

/**
 * The signal name.
 */
@property(nonatomic, readonly) NSString *name;

/**
 * The signal code.
 */
@property(nonatomic, readonly) NSString *code;

@end
