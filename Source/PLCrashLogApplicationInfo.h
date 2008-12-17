/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008 Plausible Labs Cooperative, Inc.
 * All rights reserved.
 */

#import <Cocoa/Cocoa.h>


@interface PLCrashLogApplicationInfo : NSObject {
@private
    /** Application identifier */
    NSString *_applicationIdentifier;
    
    /** Application version */
    NSString *_applicationVersion;
}

- (id) initWithApplicationIdentifier: (NSString *) applicationIdentifier 
                  applicationVersion: (NSString *) applicationVersion;

/**
 * The application identifier. This is usually the application's CFBundleIdentifier value.
 */
@property(nonatomic, readonly) NSString *applicationIdentifier;

/**
 * The application version. This is usually the application's CFBundleVersion value.
 */
@property(nonatomic, readonly) NSString *applicationVersion;

@end