//
//  GTMUnitTestDevLog.m
//  
//  Copyright 2008 Google Inc.
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

#import "GTMUnitTestDevLog.h"
#import "GTMRegex.h"
#import "GTMSenTestCase.h"

@implementation GTMUnitTestDevLog
// If unittests are ever being run on separate threads, this may need to be
// made a thread local variable.
static BOOL gTrackingEnabled = NO;

+ (NSMutableArray *)patterns {
  static NSMutableArray *patterns = nil;
  if (!patterns) {
    patterns = [[NSMutableArray array] retain];
  }
  return patterns;
}

+ (BOOL)isTrackingEnabled {
  return gTrackingEnabled;
}

+ (void)enableTracking {
  gTrackingEnabled = YES;
}

+ (void)disableTracking {
  gTrackingEnabled = NO;
}

+ (void)log:(NSString*)format, ... {
  va_list argList;
  va_start(argList, format);
  [self log:format args:argList];
  va_end(argList);
}

+ (void)log:(NSString*)format args:(va_list)args {
  if ([self isTrackingEnabled]) {
    NSString *logString = [[[NSString alloc] initWithFormat:format 
                                                  arguments:args] autorelease];
    @synchronized(self) {
      NSMutableArray *patterns = [self patterns];
      BOOL logError = [patterns count] == 0 ? YES : NO;
      GTMRegex *regex = nil;
      if (!logError) {
        regex = [[[patterns objectAtIndex:0] retain] autorelease];
        logError = [regex matchesString:logString] ? NO : YES;
        [patterns removeObjectAtIndex:0];
      }
      if (logError) {
        if (regex) {
          [NSException raise:SenTestFailureException
                      format:@"Unexpected log: %@\nExpected: %@", 
           logString, regex];
        } else {
          [NSException raise:SenTestFailureException 
                      format:@"Unexpected log: %@", logString];
        }
      } else {
        NSLog(@"Expected Log: %@", logString);
      }
    }
  } else {
    NSLogv(format, args);
  }
}

+ (void)expectString:(NSString *)format, ... {
  va_list argList;
  va_start(argList, format);
  NSString *string = [[[NSString alloc] initWithFormat:format 
                                             arguments:argList] autorelease];
  va_end(argList);
  NSString *pattern = [GTMRegex escapedPatternForString:string];
  [self expect:1 casesOfPattern:pattern];
  
}

+ (void)expectPattern:(NSString *)format, ... {
  va_list argList;
  va_start(argList, format);
  [self expect:1 casesOfPattern:format args:argList];
  va_end(argList);
}

+ (void)expect:(NSUInteger)n casesOfString:(NSString *)format, ... {
  va_list argList;
  va_start(argList, format);
  NSString *string = [[[NSString alloc] initWithFormat:format 
                                             arguments:argList] autorelease];
  va_end(argList);
  NSString *pattern = [GTMRegex escapedPatternForString:string];
  [self expect:n casesOfPattern:pattern];
}

+ (void)expect:(NSUInteger)n casesOfPattern:(NSString*)format, ... {
  va_list argList;
  va_start(argList, format);
  [self expect:n casesOfPattern:format args:argList];
  va_end(argList);
}

+ (void)expect:(NSUInteger)n 
casesOfPattern:(NSString*)format 
          args:(va_list)args {
  NSString *pattern = [[[NSString alloc] initWithFormat:format 
                                              arguments:args] autorelease];
  GTMRegex *regex = [GTMRegex regexWithPattern:pattern 
                                       options:kGTMRegexOptionSupressNewlineSupport];
  @synchronized(self) {
    NSMutableArray *patterns = [self patterns];
    for (NSUInteger i = 0; i < n; ++i) {
      [patterns addObject:regex];
    }
  }
}    

+ (void)verifyNoMoreLogsExpected {
  @synchronized(self) {
    NSMutableArray *patterns = [self patterns];
    if ([patterns count] > 0) {
      NSMutableArray *patternsCopy = [[patterns copy] autorelease];
      [self resetExpectedLogs];
      [NSException raise:SenTestFailureException
                  format:@"Logs still expected %@", patternsCopy];
    }
  }
}

+ (void)resetExpectedLogs {
  @synchronized(self) {
    NSMutableArray *patterns = [self patterns];
    [patterns removeAllObjects];
  }
}
@end
