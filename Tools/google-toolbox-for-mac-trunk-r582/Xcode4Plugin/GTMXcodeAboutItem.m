//
//  GTMXcodeAboutItem.m
//
//  Copyright 2007-2011 Google Inc.
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

#import "GTMXcodePluginManager.h"
#import <Cocoa/Cocoa.h>

// Handles the about GTM Xcode Plugin menu item in the Application menu.
@interface GTMXcodeAboutItem : GTMXcodePlugin
- (void)performAbout:(id)sender;
@end

@implementation GTMXcodeAboutItem
+ (void)load {
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  [GTMXcodePluginManager registerPluginClass:self];
  [pool release];
}

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
  NSMenu *rootMenu = [NSApp mainMenu];
  NSMenuItem *appleMenuItem = [rootMenu itemAtIndex:0];
  NSMenu *menu = [appleMenuItem submenu];
  NSMenuItem *aboutItem
      = [[NSMenuItem alloc] initWithTitle:@"About GTM Xcode Plugin"
                                   action:@selector(performAbout:)
                            keyEquivalent:@""];
  [aboutItem setTarget:self];
  [menu insertItem:aboutItem atIndex:1];
}

- (void)performAbout:(id)sender {
  NSBundle *mainBundle = [self bundle];
  NSString *creditsPath = [mainBundle pathForResource:@"Credits" ofType:@"rtf"];
  NSAttributedString *credits
    = [[[NSAttributedString alloc] initWithPath:creditsPath
                             documentAttributes:nil] autorelease];

  NSString *path = [mainBundle pathForResource:@"GTM"
                                        ofType:@"icns"];
  NSImage *icon = [[[NSImage alloc] initWithContentsOfFile:path] autorelease];
  NSDictionary *optionsDict = [NSDictionary dictionaryWithObjectsAndKeys:
      credits, @"Credits",
      [mainBundle objectForInfoDictionaryKey:@"CFBundleName"],
      @"ApplicationName",
      [mainBundle objectForInfoDictionaryKey:@"NSHumanReadableCopyright"],
      @"Copyright",
      [mainBundle objectForInfoDictionaryKey:@"CFBundleShortVersionString"],
      @"ApplicationVersion",
      @"", @"Version",
      icon, @"ApplicationIcon",
      nil];
  [NSApp orderFrontStandardAboutPanelWithOptions:optionsDict];
}

@end
