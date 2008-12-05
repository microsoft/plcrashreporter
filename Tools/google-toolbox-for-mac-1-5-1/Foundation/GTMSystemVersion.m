//
//  GTMSystemVersion.m
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

#import "GTMSystemVersion.h"
#if GTM_MACOS_SDK
#import <Carbon/Carbon.h>
#endif

static SInt32 sGTMSystemVersionMajor = 0;
static SInt32 sGTMSystemVersionMinor = 0;
static SInt32 sGTMSystemVersionBugFix = 0;

@implementation GTMSystemVersion
+ (void)initialize {
  if (self == [GTMSystemVersion class]) {
    // Gestalt is the recommended way of getting the OS version (despite a
    // comment to the contrary in the 10.4 headers and docs; see
    // <http://lists.apple.com/archives/carbon-dev/2007/Aug/msg00089.html>).
    // The iPhone doesn't have Gestalt though, so use the plist there.
#if GTM_MACOS_SDK
    require_noerr(Gestalt(gestaltSystemVersionMajor, &sGTMSystemVersionMajor), failedGestalt);
    require_noerr(Gestalt(gestaltSystemVersionMinor, &sGTMSystemVersionMinor), failedGestalt);
    require_noerr(Gestalt(gestaltSystemVersionBugFix, &sGTMSystemVersionBugFix), failedGestalt);
    
    return;

  failedGestalt:
    ;
#if MAC_OS_X_VERSION_MIN_REQUIRED <= MAC_OS_X_VERSION_10_3
    // gestaltSystemVersionMajor et al are only on 10.4 and above, so they
    // could fail when running on 10.3.
    SInt32 binaryCodedDec;
    OSStatus err = err = Gestalt(gestaltSystemVersion, &binaryCodedDec);
    _GTMDevAssert(!err, @"Unable to get version from Gestalt");

    // Note that this code will return x.9.9 for any system rev parts that are
    // greater than 9 (i.e., 10.10.10 will be 10.9.9). This shouldn't ever be a
    // problem as the code above takes care of 10.4+.
    int msb = (binaryCodedDec & 0x0000F000L) >> 12;
    msb *= 10;
    int lsb = (binaryCodedDec & 0x00000F00L) >> 8;
    sGTMSystemVersionMajor = msb + lsb;
    sGTMSystemVersionMinor = (binaryCodedDec & 0x000000F0L) >> 4;
    sGTMSystemVersionBugFix = (binaryCodedDec & 0x0000000FL);
#endif // MAC_OS_X_VERSION_MIN_REQUIRED <= MAC_OS_X_VERSION_10_3

#else // GTM_MACOS_SDK
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    NSDictionary *systemVersionPlist = [NSDictionary dictionaryWithContentsOfFile:@"/System/Library/CoreServices/SystemVersion.plist"];
    NSString *version = [systemVersionPlist objectForKey:@"ProductVersion"];
    _GTMDevAssert(version, @"Unable to get version");
    NSArray *versionInfo = [version componentsSeparatedByString:@"."];
    NSUInteger length = [versionInfo count];
    _GTMDevAssert(length > 1 && length < 4, @"Unparseable version %@", version);
    sGTMSystemVersionMajor = [[versionInfo objectAtIndex:0] intValue];
    _GTMDevAssert(sGTMSystemVersionMajor != 0, @"Unknown version for %@", version);
    sGTMSystemVersionMinor = [[versionInfo objectAtIndex:1] intValue];
    if (length == 3) {
      sGTMSystemVersionBugFix = [[versionInfo objectAtIndex:2] intValue];
    }
    [pool release];
#endif // GTM_MACOS_SDK
  }
}

+ (void)getMajor:(long*)major minor:(long*)minor bugFix:(long*)bugFix {
  if (major) {
    *major = sGTMSystemVersionMajor;
  }
  if (minor) {
    *minor = sGTMSystemVersionMinor;
  }
  if (major) {
    *bugFix = sGTMSystemVersionBugFix;
  }
}

#if GTM_MACOS_SDK
+ (BOOL)isPanther {
  return sGTMSystemVersionMajor == 10 && sGTMSystemVersionMinor == 3;
}

+ (BOOL)isTiger {
  return sGTMSystemVersionMajor == 10 && sGTMSystemVersionMinor == 4;
}

+ (BOOL)isLeopard {
  return sGTMSystemVersionMajor == 10 && sGTMSystemVersionMinor == 5;
}

+ (BOOL)isPantherOrGreater {
  return (sGTMSystemVersionMajor > 10) || 
          (sGTMSystemVersionMajor == 10 && sGTMSystemVersionMinor >= 3);
}

+ (BOOL)isTigerOrGreater {
  return (sGTMSystemVersionMajor > 10) || 
          (sGTMSystemVersionMajor == 10 && sGTMSystemVersionMinor >= 4);
}

+ (BOOL)isLeopardOrGreater {
  return (sGTMSystemVersionMajor > 10) || 
          (sGTMSystemVersionMajor == 10 && sGTMSystemVersionMinor >= 5);
}

#endif // GTM_MACOS_SDK

@end
