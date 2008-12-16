/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008 Plausible Labs Cooperative, Inc.
 * All rights reserved.
 */

#import <Foundation/Foundation.h>
#import "CrashReporter.h"

int main (int argc, char *argv[]) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    NSError *error = nil;

    /* Register the crash reporter */
    if (![[PLCrashReporter sharedReporter] enableCrashReporterAndReturnError: &error]) {
        NSLog(@"Could not enable crash reporter: %@", error);
    }

    /* Trigger a crash */
    CFRelease(NULL);

    [pool release];
}