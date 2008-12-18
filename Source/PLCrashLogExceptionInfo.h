/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008 Plausible Labs Cooperative, Inc.
 * All rights reserved.
 */

#import <Foundation/Foundation.h>


@interface PLCrashLogExceptionInfo : NSObject {
@private
    /** Name */
    NSString *_name;

    /** Reason */
    NSString *_reason;
}

- (id) initWithExceptionName: (NSString *) name reason: (NSString *) reason;

/**
 * The exception name.
 */
@property(nonatomic, readonly) NSString *exceptionName;

/**
 * The exception reason.
 */
@property(nonatomic, readonly) NSString *exceptionReason;

@end
