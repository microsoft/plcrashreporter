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

#import <Foundation/Foundation.h>
#import "CrashReporter.h"

/* A custom post-crash callback */
void post_crash_callback (siginfo_t *info, ucontext_t *uap, void *context) {
    // this is not async-safe, but this is a test implementation
    NSLog(@"post crash callback: signo=%d, uap=%p, context=%p", info->si_signo, uap, context);
}


void stackFrame (void) {
    /* Trigger a crash */
    ((char *)NULL)[1] = 0;
}

/* If a crash report exists, make it accessible via iTunes document sharing. This is a no-op on Mac OS X. */
static void save_crash_report () {
    if (![[PLCrashReporter sharedReporter] hasPendingCrashReport]) 
        return;

#if TARGET_OS_IPHONE
    NSFileManager *fm = [NSFileManager defaultManager];
    NSError *error;
    
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    NSString *documentsDirectory = [paths objectAtIndex:0];
    if (![fm createDirectoryAtPath: documentsDirectory withIntermediateDirectories: YES attributes:nil error: &error]) {
        NSLog(@"Could not create documents directory: %@", error);
        return;
    }


    NSData *data = [[PLCrashReporter sharedReporter] loadPendingCrashReportDataAndReturnError: &error];
    if (data == nil) {
        NSLog(@"Failed to load crash report data: %@", error);
        return;
    }

    NSString *outputPath = [documentsDirectory stringByAppendingPathComponent: @"demo.plcrash"];
    if (![data writeToFile: outputPath atomically: YES]) {
        NSLog(@"Failed to write crash report");
    }
    
    NSLog(@"Saved crash report to: %@", outputPath);
#endif
}

int main (int argc, char *argv[]) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    NSError *error = nil;

    /* Save any existing crash report. */
    save_crash_report();
    
    /* Set up post-crash callbacks */
    PLCrashReporterCallbacks cb = {
        .version = 0,
        .context = (void *) 0xABABABAB,
        .handleSignal = post_crash_callback
    };
    [[PLCrashReporter sharedReporter] setCrashCallbacks: &cb];

    /* Enable the crash reporter */
    if (![[PLCrashReporter sharedReporter] enableCrashReporterAndReturnError: &error]) {
        NSLog(@"Could not enable crash reporter: %@", error);
    }

    /* Add another stack frame */
    stackFrame();

    [pool release];
}
