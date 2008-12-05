//
//  GTMUnitTestingUtilities.h
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

#import <Foundation/Foundation.h>
#import <objc/objc.h>

// Collection of utilities for unit testing
@interface GTMUnitTestingUtilities : NSObject

// Returns YES if we are currently being unittested.
+ (BOOL)areWeBeingUnitTested;

// Sets up the user interface so that we can run consistent UI unittests on
// it. This includes setting scroll bar types, setting selection colors
// setting color spaces etc so that everything is consistent across machines.
// This should be called in main, before NSApplicationMain is called.
+ (void)setUpForUIUnitTests;

// Syntactic sugar combining the above, and wrapping them in an 
// NSAutoreleasePool so that your main can look like this:
// int main(int argc, const char *argv[]) {
//   [UnitTestingUtilities setUpForUIUnitTestsIfBeingTested];
//   return NSApplicationMain(argc, argv);
// }
+ (void)setUpForUIUnitTestsIfBeingTested;

@end

