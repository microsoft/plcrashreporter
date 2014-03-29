//
//  PLAppDelegate.h
//  OnErrorResumeExample
//
//  Created by Landon Fuller on 3/28/14.
//  Copyright (c) 2014 Plausible Labs. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import <SpriteKit/SpriteKit.h>

@interface PLAppDelegate : NSObject <NSApplicationDelegate>

@property (assign) IBOutlet NSWindow *window;
@property (assign) IBOutlet SKView *sceneView;

@property (assign) IBOutlet NSButton *cfReleaseButton;
@property (assign) IBOutlet NSButton *nullButton;
@property (assign) IBOutlet NSButton *msgSendButton;

@end
