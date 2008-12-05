//
//  GTMSystemVersion.h
//
//  Copyright 2007-2008 Google Inc.
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

#import <Foundation/Foundation.h>
#import "GTMDefines.h"

// A class for getting information about what system we are running on
@interface GTMSystemVersion : NSObject

// Returns the current system version major.minor.bugFix
+ (void)getMajor:(long*)major minor:(long*)minor bugFix:(long*)bugFix;

#if GTM_MACOS_SDK
// Returns YES if running on 10.3, NO otherwise.
+ (BOOL)isPanther;

// Returns YES if running on 10.4, NO otherwise.
+ (BOOL)isTiger;

// Returns YES if running on 10.5, NO otherwise.
+ (BOOL)isLeopard;

// Returns a YES/NO if the system is 10.3 or better
+ (BOOL)isPantherOrGreater;

// Returns a YES/NO if the system is 10.4 or better
+ (BOOL)isTigerOrGreater;

// Returns a YES/NO if the system is 10.5 or better
+ (BOOL)isLeopardOrGreater;
#endif  // GTM_MACOS_SDK

@end
