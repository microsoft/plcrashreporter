//
//  GTMSystemVersionTest.m
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

#import "GTMSenTestCase.h"
#import "GTMSystemVersion.h"

@interface GTMSystemVersionTest : GTMTestCase
@end

@implementation GTMSystemVersionTest
- (void)testBasics {
  long major;
  long minor;
  long bugFix;
  
  [GTMSystemVersion getMajor:nil minor:nil bugFix:nil];
  [GTMSystemVersion getMajor:&major minor:&minor bugFix:&bugFix];
#if GTM_IPHONE_SDK
  STAssertTrue(major >= 2 && minor >= 0 && bugFix >= 0, nil);
#else
  STAssertTrue(major >= 10 && minor >= 3 && bugFix >= 0, nil);
  BOOL isPanther = (major == 10) && (minor == 3);
  BOOL isTiger = (major == 10) && (minor == 4);
  BOOL isLeopard = (major == 10) && (minor == 5);
  BOOL isLater = (major > 10) || ((major == 10) && (minor > 5));
  STAssertEquals([GTMSystemVersion isPanther], isPanther, nil);
  STAssertEquals([GTMSystemVersion isPantherOrGreater],
                 (BOOL)(isPanther || isTiger || isLeopard || isLater), nil);
  STAssertEquals([GTMSystemVersion isTiger], isTiger, nil);
  STAssertEquals([GTMSystemVersion isTigerOrGreater],
                 (BOOL)(isTiger || isLeopard || isLater), nil);
  STAssertEquals([GTMSystemVersion isLeopard], isLeopard, nil);
  STAssertEquals([GTMSystemVersion isLeopardOrGreater],
                 (BOOL)(isLeopard || isLater), nil);
#endif
}

@end
