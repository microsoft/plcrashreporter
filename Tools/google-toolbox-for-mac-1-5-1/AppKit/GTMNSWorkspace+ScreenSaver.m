//
//  GTMNSWorkspace+ScreenSaver.m
//
//  Copyright 2006-2008 Google Inc.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not
//  use this file except in compliance with the License.  You may obtain a copy
//  of the License at
// 
//  http://www.apache.org/licenses/LICENSE-2.0
// 
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
//  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
//  License for the specific language governing permissions and limitations under
//  the License.
//

#import <Carbon/Carbon.h>
#import <ScreenSaver/ScreenSaver.h>
#import "GTMNSWorkspace+ScreenSaver.h"
#import "GTMDefines.h"
#import "GTMGarbageCollection.h"

// Interesting class descriptions extracted from ScreenSaver.framework using
// class-dump. Note that these are "not documented".

@protocol ScreenSaverControl

- (BOOL)screenSaverIsRunning;
- (BOOL)screenSaverCanRun;
- (void)setScreenSaverCanRun:(BOOL)fp8;
- (void)screenSaverStartNow;
- (void)screenSaverStopNow;
- (void)restartForUser:(id)fp8;
- (double)screenSaverTimeRemaining;
- (void)screenSaverDidFade;
- (BOOL)screenSaverIsRunningInBackground;
- (void)screenSaverDidFadeInBackground:(BOOL)fp8 
                                 psnHi:(unsigned int)fp12 
                                 psnLow:(unsigned int)fp16;

@end

@interface ScreenSaverController : NSObject <ScreenSaverControl> {
  NSConnection *_connection;
  id _daemonProxy;
  void *_reserved;
}

+ (id)controller;
+ (id)monitor;
+ (id)daemonConnectionName;
+ (id)enginePath;
- (void)_connectionClosed:(id)fp8;
- (id)init;
- (void)dealloc;
- (BOOL)screenSaverIsRunning;
- (BOOL)screenSaverCanRun;
- (void)setScreenSaverCanRun:(BOOL)fp8;
- (void)screenSaverStartNow;
- (void)screenSaverStopNow;
- (void)restartForUser:(id)fp8;
- (double)screenSaverTimeRemaining;
- (void)screenSaverDidFade;
- (BOOL)screenSaverIsRunningInBackground;
- (void)screenSaverDidFadeInBackground:(BOOL)fp8 
                                 psnHi:(unsigned int)fp12 
                                 psnLow:(unsigned int)fp16;

@end

// end of extraction

@implementation NSWorkspace (GTMScreenSaverAddition)
// Check if the screen saver is running.
+ (BOOL)gtm_isScreenSaverActive {
  BOOL answer = NO;
  ScreenSaverController *controller = nil;
  // We're calling into an "undocumented" framework here, so we are going to
  // step rather carefully (and in 10.5.2 it's only 32bit).

#if !__LP64__
  Class screenSaverControllerClass = NSClassFromString(@"ScreenSaverController");
  _GTMDevAssert(screenSaverControllerClass, 
                @"Are you linked with ScreenSaver.framework?"
                " Can't find ScreenSaverController class.");
  if ([screenSaverControllerClass respondsToSelector:@selector(controller)]) {
    controller = [ScreenSaverController controller];
    if (controller) {
      if ([controller respondsToSelector:@selector(screenSaverIsRunning)]) {
        answer = [controller screenSaverIsRunning];
      } else {
        // COV_NF_START
        _GTMDevLog(@"ScreenSaverController no longer supports -screenSaverIsRunning?");  
        controller = nil;
        // COV_NF_END
      }
    }
  }
#endif // !__LP64__
  
  if (!controller) {
    // COV_NF_START
    // If we can't get the controller, chances are we are being run from the
    // command line and don't have access to the window server. As such we are
    // going to fallback to the older method of figuring out if a screen saver
    // is running.
    ProcessSerialNumber psn;
    // Check if the saver is already running
    require_noerr(GetFrontProcess(&psn), CantGetFrontProcess);
    
    CFDictionaryRef cfProcessInfo 
      = ProcessInformationCopyDictionary(&psn, 
                                         kProcessDictionaryIncludeAllInformationMask);
    
    require(cfProcessInfo, CantGetFrontProcess);
    NSDictionary *processInfo = [GTMNSMakeCollectable(cfProcessInfo) autorelease];
    NSString *bundlePath = [processInfo objectForKey:@"BundlePath"];

    // ScreenSaverEngine is the frontmost app if the screen saver is actually
    // running Security Agent is the frontmost app if the "enter password"
    // dialog is showing
    answer = [bundlePath hasSuffix:@"ScreenSaverEngine.app"] || 
             [bundlePath hasSuffix:@"SecurityAgent.app"];
    // COV_NF_END
  }
CantGetFrontProcess:
  return answer;
}

@end
