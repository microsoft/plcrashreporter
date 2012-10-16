//
//  GTMIPhoneSecurityLibraryTest.m
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
//  License for the specific language governing permissions and limitations under
//  the License.
//

#import "GTMSenTestCase.h"

// Tests that using the keychain library within unit tests works correctly.
@interface GTMIPhoneSecurityLibraryTest : GTMTestCase
@end

@implementation GTMIPhoneSecurityLibraryTest

static NSString * const kAccount = @"GTMTestingSecurityAccount";
static NSString * const kService = @"GTMTestingSecurityService";
static NSString * const kPassword = @"GTMTestingSecurityPassword";

- (NSMutableDictionary *)keychainQueryItem {
  return [NSMutableDictionary dictionaryWithObjectsAndKeys:
      (id)kSecClassGenericPassword, (id)kSecClass,
      kAccount, (id)kSecAttrAccount,
      kService, (id)kSecAttrService,
      nil];
}

- (void)assertCorrectPassword {
  NSMutableDictionary *keychainQueryItem = [self keychainQueryItem];
  [keychainQueryItem setObject:(id)kCFBooleanTrue forKey:(id)kSecReturnData];
  [keychainQueryItem setObject:(id)kSecMatchLimitOne forKey:(id)kSecMatchLimit];

  CFDataRef result = NULL;
  OSStatus status = SecItemCopyMatching((CFDictionaryRef)keychainQueryItem,
                                        (CFTypeRef *)&result);
  STAssertEquals(status,
                 (OSStatus)noErr,
                 @"Error retrieving password from keychain");
  STAssertNotNULL(result, @"No password found");
  NSString *password =
      [[[NSString alloc] initWithData:(NSData *)result
                             encoding:NSUTF8StringEncoding] autorelease];
  STAssertEqualStrings(kPassword, password, @"Unexpected password found");
  CFRelease(result);
}

- (void)testSecurityCalls {
  NSMutableDictionary *passwordItem = [self keychainQueryItem];
  NSData *passwordData = [kPassword dataUsingEncoding:NSUTF8StringEncoding];
  [passwordItem setObject:passwordData forKey:(id)kSecValueData];

  OSStatus result = SecItemAdd((CFDictionaryRef)passwordItem, NULL);
  STAssertTrue(result == noErr || result == errSecDuplicateItem,
               @"Unexpected result code: %lu",
               (unsigned long)result);
  [self assertCorrectPassword];

  // Test that accessing the keychain will continue to work after a delay.
  NSDate *sleepUntil = [NSDate dateWithTimeIntervalSinceNow:30];
  [[NSRunLoop mainRunLoop] runUntilDate:sleepUntil];
  [self assertCorrectPassword];
}

@end
