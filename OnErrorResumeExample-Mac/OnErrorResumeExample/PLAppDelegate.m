//
//  PLAppDelegate.m
//  OnErrorResumeExample
//
//  Created by Landon Fuller on 3/28/14.
//  Copyright (c) 2014 Plausible Labs. All rights reserved.
//

#import "PLAppDelegate.h"

#import <CrashReporter/CrashReporter.h>

@implementation PLAppDelegate {
    PLCrashReporter *_reporter;
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
    PLCrashReporterConfig *config = [[PLCrashReporterConfig alloc] initWithSignalHandlerType: PLCrashReporterSignalHandlerTypeBSD
                                                                       symbolicationStrategy: PLCrashReporterSymbolicationStrategyNone
                                                                               onErrorResume: YES];
    
    _reporter = [[PLCrashReporter alloc] initWithConfiguration: config];
    [_reporter enableCrashReporter];
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
    /* An object cache? Maybe. Or maybe we're from another universe */
    void *cache[] = {
        NULL, NULL, NULL
    };
    
    void *displayStrings[6] = {
        "This little piggy went to the market",
        "This little piggy stayed at home",
        cache,
        "This little piggy had roast beef.",
        "This little piggy had none.",
        "And this little piggy went 'Wee! Wee! Wee!' all the way home",
    };
    
    /* A corrupted/under-retained/re-used piece of memory */
    struct {
        void *isa;
    } corruptObj;
    corruptObj.isa = displayStrings;

    /* Send a message into the unknown */
    [((__bridge id) (void *) 0x5) description];
}

@end
