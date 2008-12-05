//
//  GTMStackTrace.m
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

#include <stdlib.h>
#include <dlfcn.h>
#include <mach-o/nlist.h>
#include "GTMStackTrace.h"

// Structure representing a small portion of a stack, starting from the saved
// frame pointer, and continuing through the saved program counter.
struct StackFrame {
  void *saved_fp;
#if defined (__ppc__) || defined(__ppc64__)
  void *padding;
#endif
  void *saved_pc;
};

// __builtin_frame_address(0) is a gcc builtin that returns a pointer to the
// current frame pointer.  We then use the frame pointer to walk the stack
// picking off program counters and other saved frame pointers.  This works
// great on i386, but PPC requires a little more work because the PC (or link
// register) isn't always stored on the stack.
//   
int GTMGetStackProgramCounters(void *outPcs[], int size) {
  if (!outPcs || (size < 1)) return 0;
  
  struct StackFrame *fp;
#if defined (__ppc__) || defined(__ppc64__)
  outPcs[0] = __builtin_return_address(0);
  fp = (struct StackFrame *)__builtin_frame_address(1);
#elif defined (__i386__) || defined(__x86_64__)
  fp = (struct StackFrame *)__builtin_frame_address(0);
#else
#error architecture not supported
#endif

  int level = 0;
  while (level < size) {
    if (fp == NULL) {
      level--;
      break;
    }
    outPcs[level] = fp->saved_pc;
    level++;
    fp = (struct StackFrame *)fp->saved_fp;
  }
  
  return level;
}

CFStringRef GTMStackTraceCreate(void) {
  // The maximum number of stack frames that we will walk.  We limit this so
  // that super-duper recursive functions (or bugs) don't send us for an
  // infinite loop.
  static const int kMaxStackTraceDepth = 100;
  void *pcs[kMaxStackTraceDepth];
  int depth = kMaxStackTraceDepth;
  depth = GTMGetStackProgramCounters(pcs, depth);
  
  CFMutableStringRef trace = CFStringCreateMutable(kCFAllocatorDefault, 0);
  
  for (int i = 0; i < depth; i++) {
    Dl_info info = { NULL, NULL, NULL, NULL };
    dladdr(pcs[i], &info);
    const char *symbol = info.dli_sname;
    const char *fname  = info.dli_fname;
    
    CFStringAppendFormat(trace, NULL,
                         CFSTR("#%-2d 0x%08lx %s ()  [%s]\n"),
                         i, pcs[i],
                         (symbol ? symbol : "??"),
                         (fname  ? fname  : "??"));
  }
  
  return trace;
}
