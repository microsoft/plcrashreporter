//
//  PLCrashRecoveryWindowController.m
//  CrashReporter
//
//  Created by Landon Fuller on 3/28/14.
//
//

#import "PLCrashRecoveryWindowController.h"

@interface PLCrashRecoveryWindowController ()

@end

@implementation PLCrashRecoveryWindowController

@synthesize statusField;
@synthesize progress;

- (id) init {
    return [self initWithWindowNibName: @"PLCrashRecoveryWindowController"];
}

- (id)initWithWindow:(NSWindow *)window
{
    self = [super initWithWindow:window];
    if (self) {
        // Initialization code here.
    }
    return self;
}

- (void)windowDidLoad
{
    [super windowDidLoad];
    
    [self.progress setUsesThreadedAnimation: YES];
    [self.progress startAnimation: self];
    
    // Implement this method to handle any initialization after your window controller's window has been loaded from its nib file.
}

@end
