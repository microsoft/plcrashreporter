//
//  GTMXcodePluginManager.m
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

static NSMutableArray *gRegisteredClasses = nil;
static NSMutableArray *gPlugins = nil;

@implementation GTMXcodePluginManager
+ (void)registerPluginClass:(Class)cls {
  @synchronized([self class]) {
    if (!gRegisteredClasses) {
      gRegisteredClasses = [[NSMutableArray alloc] init];
    }
    if (cls) {
      [gRegisteredClasses addObject:cls];
    }
  }
}

+ (void)pluginDidLoad:(NSBundle *)bundle {
  gPlugins
      = [[NSMutableArray alloc] initWithCapacity:[gRegisteredClasses count]];
  for (Class cls in gRegisteredClasses) {
    GTMXcodePlugin *plugin = [[cls alloc] initWithBundle:bundle];
    if (plugin) {
      [gPlugins addObject:plugin];
    }
  }

  [gRegisteredClasses release];
  gRegisteredClasses = nil;

  NSNotificationCenter *nc = [NSNotificationCenter defaultCenter];
  [nc addObserver:self
         selector:@selector(applicationDidFinishLaunching:)
             name:NSApplicationDidFinishLaunchingNotification
           object:NSApp];
}

+ (void)applicationDidFinishLaunching:(NSNotification *)notification {
  NSNotificationCenter *nc = [NSNotificationCenter defaultCenter];
  [nc removeObserver:self
                name:NSApplicationDidFinishLaunchingNotification
              object:NSApp];
  for (GTMXcodePlugin *plugin in gPlugins) {
    [plugin applicationDidFinishLaunching:notification];
  }
}

+ (float)xCodeVersion {
  NSBundle *bundle = [NSBundle mainBundle];
  id object = [bundle objectForInfoDictionaryKey:@"CFBundleShortVersionString"];
  float value = [object floatValue];
  if (!(value > 0.0)) {
    NSLog(@"Unable to get Xcode Version");
  }
  return value;
}

@end

@implementation GTMXcodePlugin

@synthesize bundle = bundle_;

- (id)initWithBundle:(NSBundle *)bundle {
  if ((self = [super init])) {
    bundle_ = [bundle retain];
  }
  return self;
}

- (void)dealloc {
  [bundle_ release];
  [super dealloc];
}

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
}

@end

