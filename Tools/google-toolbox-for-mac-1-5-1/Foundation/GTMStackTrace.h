//
//  GTMStackTrace.h
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

#include <CoreFoundation/CoreFoundation.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Returns a string containing a nicely formatted stack trace.
// The caller owns the returned CFStringRef and is responsible for releasing it.
//
// *****************************************************************************
// The symbolic names returned for Objective-C methods will be INCORRECT. This
// is because dladdr() doesn't properly resolve Objective-C names. The symbol's
// address will be CORRECT, so will be able to use atos or gdb to get a properly
// resolved Objective-C name.  --  5/15/2007
// TODO: write dladdr() replacement that works with Objective-C symbols.
// *****************************************************************************
// 
// This function gets the stack trace for the current thread, and is safe to
// use in production multi-threaded code.  Typically this function will be used
// along with some loggins, as in the following:
//
//   MyAppLogger(@"Should never get here:\n%@", GTMStackTrace());
//
// Here is a sample stack trace returned from this function:
//
// #0  0x00002d92 D ()  [/Users/me/./StackLog]
// #1  0x00002e45 C ()  [/Users/me/./StackLog]
// #2  0x00002e53 B ()  [/Users/me/./StackLog]
// #3  0x00002e61 A ()  [/Users/me/./StackLog]
// #4  0x00002e6f main ()  [/Users/me/./StackLog]
// #5  0x00002692 tart ()  [/Users/me/./StackLog]
// #6  0x000025b9 tart ()  [/Users/me/./StackLog]
//
// If you're using this with Objective-C, you may want to use the GTMStackTrace()
// variant that autoreleases the returned string.
//
CFStringRef GTMStackTraceCreate(void);
  
/// Wrapper that autoreleases the returned CFStringRef.
// This is simply for the convenience of those using Objective-C.
//
#if __OBJC__
#include "GTMGarbageCollection.h"
#define GTMStackTrace() [GTMNSMakeCollectable(GTMStackTraceCreate()) autorelease]
#endif

/// Returns an array of program counters from the current thread's stack.
// *** You should probably use GTMStackTrace() instead of this function ***
// However, if you actually want all the PCs in "void *" form, then this
// funtion is more convenient.
//
// Args:
//   outPcs - an array of "void *" pointers to the program counters found on the
//            current thread's stack.
//   size - the size of outPcs
//
// Returns:
//   The number of program counters actually added to outPcs.
//
int GTMGetStackProgramCounters(void *outPcs[], int size);

#ifdef __cplusplus
}
#endif
