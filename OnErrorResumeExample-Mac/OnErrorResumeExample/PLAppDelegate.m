//
//  PLAppDelegate.m
//  OnErrorResumeExample
//
//  Created by Landon Fuller on 3/28/14.
//  Copyright (c) 2014 Plausible Labs. All rights reserved.
//

#import "PLAppDelegate.h"

#import <CrashReporter/CrashReporter.h>
#import "PLBouncingScene.h"

@interface PLAppDelegate () <NSWindowDelegate> @end

@implementation PLAppDelegate {
    PLCrashReporter *_reporter;
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
    PLCrashReporterConfig *config = [[PLCrashReporterConfig alloc] initWithSignalHandlerType: PLCrashReporterSignalHandlerTypeBSD
                                                                       symbolicationStrategy: PLCrashReporterSymbolicationStrategyNone
                                                                               onErrorResume: YES];
    
    _reporter = [[PLCrashReporter alloc] initWithConfiguration: config];
    [_reporter enableCrashReporter];
    
    PLBouncingScene *scene = [[PLBouncingScene alloc] initWithSize: self.sceneView.bounds.size];
    scene.scaleMode = SKSceneScaleModeAspectFill;
    [self.sceneView presentScene: scene];
    self.sceneView.paused = NO;
    self.sceneView.asynchronous = NO;
}

- (IBAction) cfreleaseMe: (id) sender {
    /* CFRelease assertion! */
    CFRelease(NULL);
}

- (IBAction) sunyata:(id)sender {
    /* I am become NULL */
    ((volatile char *) NULL)[0] = 0xFF;
}

- (IBAction) firstContact:(id)sender {
    /* Send a message into the unknown */
    [((__bridge id) (void *) 0x5) description];
}

/* OK, there's no such thing as "Reticulating Splines", and while the rest of the steps
 * shown in our "Crash Recovery" dialog *are* real, they only take a few microseconds at most.
 * The UI is all theater ... but it's cool, right?
 *
 * This code makes it appear that the animation has stopped while we're fixing the crash, but the
 * truth is, by the time these delegates are called, the crash was already fixed. */
- (void)windowDidBecomeKey:(NSNotification *)notification { self.sceneView.paused = NO; }
- (void)windowDidResignKey:(NSNotification *)notification { self.sceneView.paused = YES; }


@end
