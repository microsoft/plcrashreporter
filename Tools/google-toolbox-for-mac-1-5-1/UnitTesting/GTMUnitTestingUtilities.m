//
//  GTMUnitTestingUtilities.m
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

#import "GTMUnitTestingUtilities.h"
#import <AppKit/AppKit.h>
#import "GTMDefines.h"

// The Users profile before we change it on them
static CMProfileRef gCurrentColorProfile = NULL;

// Compares two color profiles
static BOOL AreCMProfilesEqual(CMProfileRef a, CMProfileRef b);
// Stores the user's color profile away, and changes over to generic.
static void SetColorProfileToGenericRGB();
// Restores the users profile.
static void RestoreColorProfile(void);

@implementation GTMUnitTestingUtilities

// Returns YES if we are currently being unittested.
+ (BOOL)areWeBeingUnitTested {
  BOOL answer = NO;
  
  // Check to see if the SenTestProbe class is linked in before we call it.
  Class SenTestProbeClass = NSClassFromString(@"SenTestProbe");
  if (SenTestProbeClass != Nil) {
    // Doing this little dance so we don't actually have to link
    // SenTestingKit in
    SEL selector = NSSelectorFromString(@"isTesting");
    NSMethodSignature *sig = [SenTestProbeClass methodSignatureForSelector:selector];
    NSInvocation *invocation = [NSInvocation invocationWithMethodSignature:sig];
    [invocation setSelector:selector];
    [invocation invokeWithTarget:SenTestProbeClass];
    [invocation getReturnValue:&answer];
  }
  return answer;
}

// Sets up the user interface so that we can run consistent UI unittests on it.
+ (void)setUpForUIUnitTests {
  // Give some names to undocumented defaults values
  const NSInteger MediumFontSmoothing = 2;
  const NSInteger BlueTintedAppearance = 1;
  
  // This sets up some basic values that we want as our defaults for doing pixel
  // based user interface tests. These defaults only apply to the unit test app,
  // except or the color profile which will be set system wide, and then
  // restored when the tests complete.
  NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
  // Scroll arrows together bottom
  [defaults setObject:@"DoubleMax" forKey:@"AppleScrollBarVariant"];
  // Smallest font size to CG should perform antialiasing on
  [defaults setInteger:4 forKey:@"AppleAntiAliasingThreshold"];
  // Type of smoothing
  [defaults setInteger:MediumFontSmoothing forKey:@"AppleFontSmoothing"];
  // Blue aqua
  [defaults setInteger:BlueTintedAppearance forKey:@"AppleAquaColorVariant"];
  // Standard highlight colors
  [defaults setObject:@"0.709800 0.835300 1.000000" 
               forKey:@"AppleHighlightColor"];
  [defaults setObject:@"0.500000 0.500000 0.500000" 
               forKey:@"AppleOtherHighlightColor"];
  // Use english plz
  [defaults setObject:[NSArray arrayWithObject:@"en"] forKey:@"AppleLanguages"];
  // How fast should we draw sheets. This speeds up the sheet tests considerably
  [defaults setFloat:.001f forKey:@"NSWindowResizeTime"];
  // Switch over the screen profile to "generic rgb". This installs an 
  // atexit handler to return our profile back when we are done.
  SetColorProfileToGenericRGB();  
}

+ (void)setUpForUIUnitTestsIfBeingTested {
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  if ([self areWeBeingUnitTested]) {
    [self setUpForUIUnitTests];
  }
  [pool release];
}
@end


BOOL AreCMProfilesEqual(CMProfileRef a, CMProfileRef b) {
  BOOL equal = YES;
  if (a != b) {
    CMProfileMD5 aMD5;
    CMProfileMD5 bMD5;
    CMError aMD5Err = CMGetProfileMD5(a, aMD5);
    CMError bMD5Err = CMGetProfileMD5(b, bMD5);
    equal = (!aMD5Err && 
            !bMD5Err && 
            !memcmp(aMD5, bMD5, sizeof(CMProfileMD5))) ? YES : NO;
  }
  return equal;
}

static void RestoreColorProfile(void) {
  if (gCurrentColorProfile) {
    CGDirectDisplayID displayID = CGMainDisplayID();
    CMError error = CMSetProfileByAVID((UInt32)displayID, gCurrentColorProfile);
    if (error) {
      // COV_NF_START
      // No way to force this case in a unittest.
      _GTMDevLog(@"Failed to restore previous color profile! "
            "You may need to open System Preferences : Displays : Color "
            "and manually restore your color settings. (Error: %i)", error);
      // COV_NF_END      
    } else {
      _GTMDevLog(@"Color profile restored");
    }
    gCurrentColorProfile = NULL;
  }
}

void SetColorProfileToGenericRGB(void) {
  NSColorSpace *genericSpace = [NSColorSpace genericRGBColorSpace];
  CMProfileRef genericProfile = (CMProfileRef)[genericSpace colorSyncProfile];
  CMProfileRef previousProfile;
  CGDirectDisplayID displayID = CGMainDisplayID();
  CMError error = CMGetProfileByAVID((UInt32)displayID, &previousProfile);
  if (error) {
    // COV_NF_START
    // No way to force this case in a unittest.
    _GTMDevLog(@"Failed to get current color profile. "
               "I will not be able to restore your current profile, thus I'm "
               "not changing it. Many unit tests may fail as a result. (Error: %i)", 
          error);
    return;
    // COV_NF_END
  }
  if (AreCMProfilesEqual(genericProfile, previousProfile)) {
    return;
  }
  CFStringRef previousProfileName;
  CFStringRef genericProfileName;
  CMCopyProfileDescriptionString(previousProfile, &previousProfileName);
  CMCopyProfileDescriptionString(genericProfile, &genericProfileName);
  
  _GTMDevLog(@"Temporarily changing your system color profile from \"%@\" to \"%@\".", 
             previousProfileName, genericProfileName);
  _GTMDevLog(@"This allows the pixel-based unit-tests to have consistent color "
             "values across all machines.");
  _GTMDevLog(@"The colors on your screen will change for the duration of the testing.");
  
  
  if ((error = CMSetProfileByAVID((UInt32)displayID, genericProfile))) {
    // COV_NF_START
    // No way to force this case in a unittest.
    _GTMDevLog(@"Failed to set color profile to \"%@\"! Many unit tests will fail as "
               "a result.  (Error: %i)", genericProfileName, error);
    // COV_NF_END
  } else {
    gCurrentColorProfile = previousProfile;
    atexit(RestoreColorProfile);
  }
  CFRelease(previousProfileName);
  CFRelease(genericProfileName);
}
