//
//  GTMNSWorkspace+ScreenSaver.h
//
//  Category for seeing if the screen saver is running.
//  Requires linkage with the ScreenSaver.framework. Warning, uses some 
//  undocumented methods in the ScreenSaver.framework.
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

#import <Cocoa/Cocoa.h>

@interface NSWorkspace (GTMScreenSaverAddition)

// Check if the screen saver is running. 
// Returns YES if it is running.
// Requires linking to the ScreenSaver.framework.
+ (BOOL)gtm_isScreenSaverActive;

@end

