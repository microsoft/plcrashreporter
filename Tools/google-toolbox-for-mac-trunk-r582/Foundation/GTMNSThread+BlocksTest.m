//
//  GTMNSThread+BlocksTest.m
//
//  Copyright 2012 Google Inc.
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
//  License for the specific language governing permissions and limitations
//  under the License.
//

#import "GTMSenTestCase.h"
#import "GTMNSThread+Blocks.h"

#if GTM_IPHONE_SDK || (MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_5)

#import "GTMFoundationUnitTestingUtilities.h"

@interface GTMNSThread_BlocksTest : GTMTestCase {
 @private
  NSThread *workerThread_;
  BOOL workerRunning_;
}

@property (nonatomic, readwrite, getter=isWorkerRunning) BOOL workerRunning;
@end

@implementation GTMNSThread_BlocksTest

@synthesize workerRunning = workerRunning_;

- (void)stopTestRunning:(GTMUnitTestingBooleanRunLoopContext *)context{
  [context setShouldStop:YES];
}

- (void)workerMain:(id)object {
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  while ([self isWorkerRunning]) {
    [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode
                             beforeDate:[NSDate distantFuture]];
  }
  [pool drain];
}

- (void)killWorkerThread:(GTMUnitTestingBooleanRunLoopContext *)context {
  [self setWorkerRunning:NO];
  [context setShouldStop:YES];
}

- (void)setUp {
  [self setWorkerRunning:YES];
  workerThread_ = [[NSThread alloc] initWithTarget:self
                                          selector:@selector(workerMain:)
                                            object:nil];
  [workerThread_ start];
}

- (void)tearDown {
  GTMUnitTestingBooleanRunLoopContext *context
      = [GTMUnitTestingBooleanRunLoopContext context];
  [self performSelector:@selector(killWorkerThread:)
               onThread:workerThread_
             withObject:context
          waitUntilDone:NO];
  NSRunLoop *runLoop = [NSRunLoop currentRunLoop];
  STAssertTrue([runLoop gtm_runUpToSixtySecondsWithContext:context], nil);
  [workerThread_ release];
}

- (void)testPerformBlock {
  NSThread *currentThread = [NSThread currentThread];
  GTMUnitTestingBooleanRunLoopContext *context
      = [GTMUnitTestingBooleanRunLoopContext context];
  [workerThread_ gtm_performBlock:^{
    [self performSelector:@selector(stopTestRunning:)
                 onThread:currentThread
               withObject:context
            waitUntilDone:YES];
  }];
  NSRunLoop *runLoop = [NSRunLoop currentRunLoop];
  STAssertTrue([runLoop gtm_runUpToSixtySecondsWithContext:context], nil);
}

- (void)testPerformBlockWaitUntilDone {
  GTMUnitTestingBooleanRunLoopContext *context
      = [GTMUnitTestingBooleanRunLoopContext context];
  [workerThread_ gtm_performWaitingUntilDone:YES block:^{
    [context setShouldStop:YES];
  }];
  STAssertTrue([context shouldStop], nil);
}

- (void)testPerformBlockInBackground {
  NSThread *currentThread = [NSThread currentThread];
  GTMUnitTestingBooleanRunLoopContext *context
      = [GTMUnitTestingBooleanRunLoopContext context];
  [NSThread gtm_performBlockInBackground:^{
    [self performSelector:@selector(stopTestRunning:)
                 onThread:currentThread
               withObject:context
            waitUntilDone:YES];
  }];
  NSRunLoop *runLoop = [NSRunLoop currentRunLoop];
  STAssertTrue([runLoop gtm_runUpToSixtySecondsWithContext:context], nil);
}

@end

#endif  // GTM_IPHONE_SDK || (MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_5)
