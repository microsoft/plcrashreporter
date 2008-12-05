//
//  NSAppleScript+HandlerTest.m
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

#import "GTMSenTestCase.h"
#import <Carbon/Carbon.h>
#import "GTMNSAppleScript+Handler.h"
#import "GTMNSAppleEventDescriptor+Foundation.h"
#import "GTMUnitTestDevLog.h"

@interface GTMNSAppleScript_HandlerTest : GTMTestCase {
  NSAppleScript *script_; 
}
@end

@implementation GTMNSAppleScript_HandlerTest
- (void)setUp {
  NSBundle *bundle = [NSBundle bundleForClass:[GTMNSAppleScript_HandlerTest class]];
  STAssertNotNil(bundle, nil);
  NSString *path = [bundle pathForResource:@"GTMNSAppleEvent+HandlerTest" 
                                    ofType:@"scpt"
                               inDirectory:@"Scripts"];
  STAssertNotNil(path, [bundle description]);
  NSDictionary *error = nil;
  script_ = [[NSAppleScript alloc] initWithContentsOfURL:[NSURL fileURLWithPath:path]
                                                   error:&error];
  STAssertNotNil(script_, [error description]);
  STAssertNil(error, @"Error should be nil. Error = %@", [error description]);
}

- (void)tearDown {
  [script_ release];
  script_ = nil;
}

- (void)testHandlerNoParamsNoReturn {
  NSDictionary *error = nil;
  NSAppleEventDescriptor *desc = [script_ gtm_executePositionalHandler:@"test" 
                                                            parameters:nil 
                                                                 error:&error];
  STAssertNotNil(desc, [error description]);
  STAssertNil(error, @"Error should be nil. Error = %@", [error description]);
  STAssertEquals([desc descriptorType], (DescType)typeNull, nil);
  desc = [script_ gtm_executePositionalHandler:@"test" 
                                    parameters:[NSArray array] 
                                         error:&error];
  STAssertNotNil(desc, [error description]);
  STAssertNil(error, @"Error should be nil. Error = %@", [error description]);
  STAssertEquals([desc descriptorType], (DescType)typeNull, nil);
  
  //Applescript doesn't appear to get upset about extra params
  desc = [script_ gtm_executePositionalHandler:@"test" 
                                    parameters:[NSArray arrayWithObject:@"foo"] 
                                         error:&error];
  STAssertNotNil(desc, [error description]);
  STAssertNil(error, @"Error should be nil. Error = %@", [error description]);
  STAssertEquals([desc descriptorType], (DescType)typeNull, nil);
}
  
- (void)testHandlerNoParamsWithReturn {
  NSDictionary *error = nil;
  NSAppleEventDescriptor *desc = [script_ gtm_executePositionalHandler:@"testReturnOne" 
                                                            parameters:nil 
                                                                 error:&error];
  STAssertNotNil(desc, [error description]);
  STAssertNil(error, @"Error should be nil. Error = %@", [error description]);
  STAssertEquals([desc descriptorType], (DescType)typeSInt32, nil);
  STAssertEquals([desc int32Value], (SInt32)1, nil);
  desc = [script_ gtm_executePositionalHandler:@"testReturnOne" 
                                    parameters:[NSArray array] 
                                         error:&error];
  STAssertNotNil(desc, [error description]);
  STAssertNil(error, @"Error should be nil. Error = %@", [error description]);
  STAssertEquals([desc descriptorType], (DescType)typeSInt32, nil);
  STAssertEquals([desc int32Value], (SInt32)1, nil);
  
  //Applescript doesn't appear to get upset about extra params
  desc = [script_ gtm_executePositionalHandler:@"testReturnOne" 
                                    parameters:[NSArray arrayWithObject:@"foo"] 
                                         error:&error];
  STAssertNotNil(desc, [error description]);
  STAssertNil(error, @"Error should be nil. Error = %@", [error description]);
  STAssertEquals([desc descriptorType], (DescType)typeSInt32, nil);
  STAssertEquals([desc int32Value], (SInt32)1, nil);
}

- (void)testHandlerOneParamWithReturn {
  NSDictionary *error = nil;
  // Note case change in executeHandler call
  NSAppleEventDescriptor *desc = [script_ gtm_executePositionalHandler:@"testreturnParam" 
                                                            parameters:nil 
                                                                error:&error];
  STAssertNil(desc, @"Desc should by nil %@", desc);
  STAssertNotNil(error, nil);
  error = nil;
  
  desc = [script_ gtm_executePositionalHandler:@"testReturnParam" 
                                    parameters:[NSArray array] 
                                         error:&error];
  STAssertNil(desc, @"Desc should by nil %@", desc);
  STAssertNotNil(error, nil);
  error = nil;
  
  desc = [script_ gtm_executePositionalHandler:@"testReturnParam" 
                                    parameters:[NSArray arrayWithObject:@"foo"]
                                         error:&error];
  STAssertNotNil(desc, [error description]);
  STAssertNil(error, @"Error should be nil. Error = %@", [error description]);
  STAssertEquals([desc descriptorType], (DescType)typeUnicodeText, nil);
  STAssertEqualObjects([desc gtm_objectValue], @"foo", nil);
}

- (void)testHandlerTwoParamsWithReturn {
  NSDictionary *error = nil;
  // Note case change in executeHandler call
  // Test case and empty params
  NSAppleEventDescriptor *desc = [script_ gtm_executePositionalHandler:@"testADDPArams" 
                                                            parameters:nil 
                                                                 error:&error];
  STAssertNil(desc, @"Desc should by nil %@", desc);
  STAssertNotNil(error, nil);
  
  // Test empty params
  error = nil;
  desc = [script_ gtm_executePositionalHandler:@"testAddParams" 
                                    parameters:[NSArray array] 
                                         error:&error];
  STAssertNil(desc, @"Desc should by nil %@", desc);
  STAssertNotNil(error, nil);
  
  error = nil;
  NSArray *args = [NSArray arrayWithObjects:
    [NSNumber numberWithInt:1],
    [NSNumber numberWithInt:2],
    nil];
  desc = [script_ gtm_executePositionalHandler:@"testAddParams" 
                                    parameters:args 
                                         error:&error];
  STAssertNotNil(desc, [error description]);
  STAssertNil(error, @"Error should be nil. Error = %@", [error description]);
  STAssertEquals([desc descriptorType], (DescType)typeSInt32, nil);
  STAssertEquals([desc int32Value], (SInt32)3, nil);

  // Test bad params
  error = nil;
  args = [NSArray arrayWithObjects:
    @"foo",
    @"bar",
    nil];
  desc = [script_ gtm_executePositionalHandler:@"testAddParams" 
                                    parameters:args 
                                         error:&error];
  STAssertNil(desc, @"Desc should by nil %@", desc);
  STAssertNotNil(error, nil);

  // Test too many params. Currently Applescript allows this so it should pass
  error = nil;
  args = [NSArray arrayWithObjects:
    [NSNumber numberWithInt:1],
    [NSNumber numberWithInt:2],
    [NSNumber numberWithInt:3],
    nil];
  desc = [script_ gtm_executePositionalHandler:@"testAddParams" 
                                    parameters:args 
                                         error:&error];
  STAssertNotNil(desc, [error description]);
  STAssertNil(error, @"Error should be nil. Error = %@", [error description]);
  STAssertEquals([desc descriptorType], (DescType)typeSInt32, nil);
  STAssertEquals([desc int32Value], (SInt32)3, nil);}

- (void)testLabeledHandler {
  NSDictionary *error = nil;
  AEKeyword labels[] = { keyDirectObject, 
                         keyASPrepositionOnto, 
                         keyASPrepositionGiven };
  id params[3];
  params[0] = [NSNumber numberWithInt:1];
  params[1] = [NSNumber numberWithInt:3];
  params[2] = [NSDictionary dictionaryWithObject:[NSNumber numberWithInt:4] 
                                          forKey:@"othervalue"];
  
  NSAppleEventDescriptor *desc = [script_ gtm_executeLabeledHandler:@"testAdd" 
                                                             labels:labels
                                                         parameters:params
                                                              count:sizeof(params) / sizeof(id)
                                                              error:&error];
  STAssertNotNil(desc, [error description]);
  STAssertNil(error, @"Error should be nil. Error = %@", [error description]);
  STAssertEquals([desc descriptorType], (DescType)typeSInt32, nil);
  STAssertEquals([desc int32Value], (SInt32)8, nil);
  
  // Test too many params. Currently Applescript allows this so it should pass
  AEKeyword labels2[] = { keyDirectObject, 
                         keyASPrepositionOnto, 
                         keyASPrepositionBetween,
                         keyASPrepositionGiven };
  id params2[4];
  params2[0] = [NSNumber numberWithInt:1];
  params2[1] = [NSNumber numberWithInt:3];
  params2[2] = [NSNumber numberWithInt:5];
  params2[3] = [NSDictionary dictionaryWithObject:[NSNumber numberWithInt:4] 
                                            forKey:@"othervalue"];

  error = nil;
  desc = [script_ gtm_executeLabeledHandler:@"testAdd" 
                                     labels:labels2
                                 parameters:params2
                                      count:sizeof(params2) / sizeof(id)
                                      error:&error];
  STAssertNotNil(desc, [error description]);
  STAssertNil(error, @"Error should be nil. Error = %@", [error description]);
  STAssertEquals([desc descriptorType], (DescType)typeSInt32, nil);
  STAssertEquals([desc int32Value], (SInt32)8, nil);}

- (void)testHandlers {
  NSSet *handlers = [script_ gtm_handlers];
  NSSet *expected = [NSSet setWithObjects:
                     @"aevtodoc",
                     @"test",
                     @"testreturnone",
                     @"testreturnparam",
                     @"testaddparams",
                     @"testadd",
                     nil];
  STAssertEqualObjects(handlers, expected, @"Unexpected handlers?");
}

- (void)testProperties {
  NSSet *properties = [script_ gtm_properties];
  NSSet *expected = [NSSet setWithObjects:
                     @"foo", 
                     @"asdscriptuniqueidentifier", 
                     nil];
  STAssertEqualObjects(properties, expected, @"Unexpected properties?");
  id value = [script_ gtm_valueForProperty:@"foo"];
  STAssertEqualObjects(value, [NSNumber numberWithInt:1], @"bad property?");
  BOOL goodSet = [script_ gtm_setValue:@"bar" forProperty:@"foo"];
  STAssertTrue(goodSet, @"Couldn't set property");
  value = [script_ gtm_valueForProperty:@"foo"];
  STAssertEqualObjects(value, @"bar", @"bad property?");
  
  [GTMUnitTestDevLog expectPattern:@"Unable to setValue:bar forProperty:"
   "\\(null\\) from <NSAppleScript: 0x[0-9a-f]+> \\(-50\\)"];
  goodSet = [script_ gtm_setValue:@"bar" forProperty:nil];
  STAssertFalse(goodSet, @"Set property?");
  [GTMUnitTestDevLog expectPattern:@"Unable to get valueForProperty:gargle "
   "from <NSAppleScript: 0x[0-9a-f]+> \\(-1753\\)"];
  value = [script_ gtm_valueForProperty:@"gargle"];
  STAssertNil(value, @"Property named gargle?");
}

- (void)testFailures {
  NSDictionary *error = nil;
  NSAppleEventDescriptor *desc = [script_ gtm_executePositionalHandler:@"noSuchTest" 
                                                            parameters:nil 
                                                                 error:&error];
  STAssertNil(desc, nil);
  STAssertNotNil(error, nil);

  // Test with empty handler name
  error = nil;
  desc = [script_ gtm_executePositionalHandler:@"" 
                                    parameters:[NSArray array] 
                                         error:&error];
  STAssertNil(desc, nil);
  STAssertNotNil(error, nil);
  
  // Test with nil handler
  error = nil;
  desc = [script_ gtm_executePositionalHandler:nil
                                    parameters:[NSArray array] 
                                         error:&error];
  STAssertNil(desc, nil);
  STAssertNotNil(error, nil);
  
  // Test with nil handler and nil error
  desc = [script_ gtm_executePositionalHandler:nil
                                    parameters:nil 
                                         error:nil];
  STAssertNil(desc, nil);
  
  // Test with a bad script
  NSAppleScript *script = [[[NSAppleScript alloc] initWithSource:@"david hasselhoff"] autorelease];
  [GTMUnitTestDevLog expectPattern:@"Unable to compile script: .*"];
  [GTMUnitTestDevLog expectPattern:@"Error getting handlers: -[0-9]+"];
  NSSet *handlers = [script gtm_handlers];
  STAssertEquals([handlers count], (NSUInteger)0, @"Should have no handlers");
  [GTMUnitTestDevLog expectPattern:@"Unable to compile script: .*"];
  [GTMUnitTestDevLog expectPattern:@"Error getting properties: -[0-9]+"];
  NSSet *properties = [script gtm_properties];
  STAssertEquals([properties count], (NSUInteger)0, @"Should have no properties");
}

- (void)testScriptDescriptors {
  NSAppleEventDescriptor *desc = [script_ gtm_appleEventDescriptor];
  STAssertNotNil(desc, @"Couldn't make a script desc");
  NSAppleScript *script = [desc gtm_objectValue];
  STAssertNotNil(script, @"Couldn't get a script back");
  NSSet *handlers = [script gtm_handlers];
  STAssertNotNil(handlers, @"Couldn't get handlers");
}

@protocol ScriptInterface
- (id)test;
- (id)testReturnParam:(id)param;
- (id)testAddParams:(id)param1 :(id)param2;
@end

- (void)testForwarding {
  id<ScriptInterface> foo = (id<ScriptInterface>)script_;
  [foo test];
  NSNumber *val = [foo testReturnParam:[NSNumber numberWithInt:2]];
  STAssertEquals([val intValue], 2, @"should be 2");
  val = [foo testAddParams:[NSNumber numberWithInt:2] :[NSNumber numberWithInt:3]];
  STAssertEquals([val intValue], 5, @"should be 5");
}
@end
