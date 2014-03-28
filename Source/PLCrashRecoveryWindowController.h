//
//  PLCrashRecoveryWindowController.h
//  CrashReporter
//
//  Created by Landon Fuller on 3/28/14.
//
//

#import <Cocoa/Cocoa.h>

@interface PLCrashRecoveryWindowController : NSWindowController {
    IBOutlet NSTextField *statusField;
    IBOutlet NSProgressIndicator *progress;
}

@property(retain) NSTextField *statusField;
@property(retain) NSProgressIndicator *progress;

@end
