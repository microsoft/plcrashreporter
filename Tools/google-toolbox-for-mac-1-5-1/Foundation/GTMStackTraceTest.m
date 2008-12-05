//
//  GTMStackTraceTest.m
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

#import <SenTestingKit/SenTestingKit.h>
#import <Foundation/Foundation.h>
#import "GTMStackTrace.h"
#import "GTMSenTestCase.h"

@interface GTMStackTraceTest : GTMTestCase
@end

@implementation GTMStackTraceTest

- (void)testStackTraceBasic {
  NSString *stacktrace = GTMStackTrace();
  NSArray *stacklines = [stacktrace componentsSeparatedByString:@"\n"];

  STAssertGreaterThan([stacklines count], (NSUInteger)3,
                      @"stack trace must have > 3 lines");
  STAssertLessThan([stacklines count], (NSUInteger)25,
                   @"stack trace must have < 25 lines");
  
  NSString *firstFrame = [stacklines objectAtIndex:0];
  NSRange range = [firstFrame rangeOfString:@"GTMStackTrace"];
  STAssertNotEquals(range.location, (NSUInteger)NSNotFound,
                    @"First frame should contain GTMStackTrace, stack trace: %@",
                    stacktrace);
}

- (void)testProgramCountersBasic {
  void *pcs[10];
  int depth = 10;
  depth = GTMGetStackProgramCounters(pcs, depth);
  
  STAssertGreaterThan(depth, 3, @"stack trace must have > 3 lines");
  STAssertLessThanOrEqual(depth, 10, @"stack trace must have < 10 lines");
  
  // pcs is an array of program counters from the stack.  pcs[0] should match
  // the call into GTMGetStackProgramCounters, which is tough for us to check.
  // However, we can verify that pcs[1] is equal to our current return address
  // for our current function.
  void *current_pc = __builtin_return_address(0);
  STAssertEquals(pcs[1], current_pc, @"pcs[1] should equal the current PC");
}

- (void)testProgramCountersMore {
  void *pcs0[0];
  int depth0 = 0;
  depth0 = GTMGetStackProgramCounters(pcs0, depth0);
  STAssertEquals(depth0, 0, @"stack trace must have 0 lines");

  void *pcs1[1];
  int depth1 = 1;
  depth1 = GTMGetStackProgramCounters(pcs1, depth1);
  STAssertEquals(depth1, 1, @"stack trace must have 1 lines");
  
  void *pcs2[2];
  int depth2 = 2;
  depth2 = GTMGetStackProgramCounters(pcs2, depth2);
  STAssertEquals(depth2, 2, @"stack trace must have 2 lines");
  void *current_pc = __builtin_return_address(0);
  STAssertEquals(pcs2[1], current_pc, @"pcs[1] should equal the current PC");
}

@end
