//
//  NSAppleScript+Handler.m
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

#import <Carbon/Carbon.h>
#import "GTMNSAppleScript+Handler.h"
#import "GTMNSAppleEventDescriptor+Foundation.h"
#import "GTMNSAppleEventDescriptor+Handler.h"
#import "GTMFourCharCode.h"
#import "GTMMethodCheck.h"

// Some private methods that we need to call
@interface NSAppleScript (NSPrivate)
+ (ComponentInstance)_defaultScriptingComponent;
- (OSAID) _compiledScriptID;
- (id)_initWithData:(NSData*)data error:(NSDictionary**)error;
@end

@interface NSMethodSignature (NSPrivate)
+ (id)signatureWithObjCTypes:(const char *)fp8;
@end

@implementation NSAppleScript(GTMAppleScriptHandlerAdditions)
GTM_METHOD_CHECK(NSAppleEventDescriptor, gtm_descriptorWithPositionalHandler:parametersArray:);  // COV_NF_LINE
GTM_METHOD_CHECK(NSAppleEventDescriptor, gtm_descriptorWithLabeledHandler:labels:parameters:count:);  // COV_NF_LINE
GTM_METHOD_CHECK(NSAppleEventDescriptor, gtm_registerSelector:forTypes:count:);  // COV_NF_LINE

+ (void)load {
  DescType types[] = { 
    typeScript
  };
  
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  [NSAppleEventDescriptor gtm_registerSelector:@selector(gtm_scriptValue)
                                      forTypes:types
                                         count:sizeof(types)/sizeof(DescType)];
  
  DescType types2[] = { 
    'evnt'  // No type code for this one
  };
  
  [NSAppleEventDescriptor gtm_registerSelector:@selector(gtm_eventValue)
                                      forTypes:types2
                                         count:sizeof(types2)/sizeof(DescType)];
  [pool release];
}

- (OSAID)gtm_realIDAndComponent:(ComponentInstance*)component {
  if (![self isCompiled]) {
    NSDictionary *error;
    if (![self compileAndReturnError:&error]) {
      _GTMDevLog(@"Unable to compile script: %@ %@", self, error);
      return kOSANullScript;
    }
  }
  OSAID genericID = [self _compiledScriptID];
  ComponentInstance genericComponent = [NSAppleScript _defaultScriptingComponent];
  OSAError err = OSAGenericToRealID(genericComponent, &genericID, component);
  if (err) {
    _GTMDevLog(@"Unable to get real id script: %@ %@", self, err); // COV_NF_LINE
    genericID = kOSANullScript; // COV_NF_LINE
  }
  return genericID;
}

- (NSAppleEventDescriptor*)gtm_executePositionalHandler:(NSString*)handler 
                                             parameters:(NSArray*)params 
                                                  error:(NSDictionary**)error {
  NSAppleEventDescriptor *event 
    = [NSAppleEventDescriptor gtm_descriptorWithPositionalHandler:handler 
                                                  parametersArray:params];
  return [self executeAppleEvent:event error:error];
} 

- (NSAppleEventDescriptor*)gtm_executeLabeledHandler:(NSString*)handler
                                              labels:(AEKeyword*)labels
                                          parameters:(id*)params
                                               count:(NSUInteger)count
                                               error:(NSDictionary **)error {
  NSAppleEventDescriptor *event 
    = [NSAppleEventDescriptor gtm_descriptorWithLabeledHandler:handler 
                                                        labels:labels
                                                    parameters:params
                                                         count:count];
  return [self executeAppleEvent:event error:error];
} 

- (NSSet*)gtm_handlers {
  AEDescList names = { typeNull, NULL };
  NSArray *array = nil;
  ComponentInstance component;
  OSAID osaID = [self gtm_realIDAndComponent:&component];
  OSAError err = OSAGetHandlerNames(component, kOSAModeNull, osaID, &names);
  if (!err) {
    NSAppleEventDescriptor *desc 
      = [[[NSAppleEventDescriptor alloc] initWithAEDescNoCopy:&names] autorelease];
    array = [desc gtm_objectValue];
  }
  if (err) {
    _GTMDevLog(@"Error getting handlers: %d", err);
  }
  return [NSSet setWithArray:array];
}

- (NSSet*)gtm_properties {
  AEDescList names = { typeNull, NULL };
  NSArray *array = nil;
  ComponentInstance component;
  OSAID osaID = [self gtm_realIDAndComponent:&component];
  OSAError err = OSAGetPropertyNames(component, kOSAModeNull, osaID, &names);
  if (!err) {
    NSAppleEventDescriptor *desc 
      = [[[NSAppleEventDescriptor alloc] initWithAEDescNoCopy:&names] autorelease];
    array = [desc gtm_objectValue];
  }
  if (err) {
    _GTMDevLog(@"Error getting properties: %d", err);
  }
  return [NSSet setWithArray:array];
}

- (BOOL)gtm_setValue:(id)value forProperty:(NSString*)property {
  BOOL wasGood = NO;
  NSAppleEventDescriptor *desc = [value gtm_appleEventDescriptor];
  NSAppleEventDescriptor *propertyName = [property gtm_appleEventDescriptor];
  OSAError error = paramErr;
  if (desc && propertyName) {
    OSAID valueID = kOSANullScript;
    ComponentInstance component;
    OSAID scriptID = [self gtm_realIDAndComponent:&component];
    error = OSACoerceFromDesc(component,
                              [desc aeDesc], 
                              kOSAModeNull, 
                              &valueID);
    if (!error) {
      error = OSASetProperty(component, kOSAModeNull, 
                             scriptID, [propertyName aeDesc], valueID); 
      if (!error) {
        wasGood = YES;
      }
    }
  }
  if (!wasGood) {
    _GTMDevLog(@"Unable to setValue:%@ forProperty:%@ from %@ (%d)", 
               value, property, self, error);
  }
  return wasGood;
}

- (id)gtm_valueForProperty:(NSString*)property {
  id value = nil;
  NSAppleEventDescriptor *propertyName = [property gtm_appleEventDescriptor];
  OSAError error = paramErr;
  if (propertyName) {
    ComponentInstance component;
    OSAID scriptID = [self gtm_realIDAndComponent:&component];
    OSAID valueID = kOSANullScript;
    error = OSAGetProperty(component, 
                           kOSAModeNull, 
                           scriptID, 
                           [propertyName aeDesc], 
                           &valueID);
    if (!error) {
      AEDesc aeDesc;
      error = OSACoerceToDesc(component, 
                              valueID, 
                              typeWildCard, 
                              kOSAModeNull, 
                              &aeDesc);
      if (!error) {
        NSAppleEventDescriptor *desc 
        = [[[NSAppleEventDescriptor alloc] initWithAEDescNoCopy:&aeDesc] autorelease];
        value = [desc gtm_objectValue];
      }
    }
  }
  if (error) {
    _GTMDevLog(@"Unable to get valueForProperty:%@ from %@ (%d)", 
               property, self, error);
  }
  return value;
}

- (NSAppleEventDescriptor*)gtm_appleEventDescriptor {
  ComponentInstance component;
  OSAID osaID = [self gtm_realIDAndComponent:&component];
  AEDesc result = { typeNull, NULL };
  NSAppleEventDescriptor *desc = nil;
  OSAError err = OSACoerceToDesc(component, 
                                 osaID, 
                                 typeScript,
                                 kOSAModeNull,
                                 &result);
  if (!err) {
    desc = [[[NSAppleEventDescriptor alloc] initWithAEDescNoCopy:&result] autorelease];
  } else {
    _GTMDevLog(@"Unable to coerce script %d", err);  // COV_NF_LINE
  }
  return desc;
}

- (NSMethodSignature *)methodSignatureForSelector:(SEL)aSelector {
  NSMethodSignature *signature = [super methodSignatureForSelector:aSelector];
  if (!signature) {
    NSMutableString *types = [NSMutableString stringWithString:@"@@:"];
    NSString *selName = NSStringFromSelector(aSelector);
    NSArray *selArray = [selName componentsSeparatedByString:@":"];
    NSUInteger count = [selArray count];
    for (NSUInteger i = 1; i < count; i++) {
      [types appendString:@"@"];
    }
    signature = [NSMethodSignature signatureWithObjCTypes:[types UTF8String]];    
  }
  return signature;
}

- (void)forwardInvocation:(NSInvocation *)invocation {
  SEL sel = [invocation selector];
  NSMutableString *handlerName = [NSStringFromSelector(sel) mutableCopy];
  NSUInteger handlerOrigLength = [handlerName length];
  [handlerName replaceOccurrencesOfString:@":" 
                               withString:@""
                                  options:0
                                    range:NSMakeRange(0,handlerOrigLength)];
  NSUInteger argCount = handlerOrigLength - [handlerName length];
  NSMutableArray *args = [NSMutableArray arrayWithCapacity:argCount];
  for (NSUInteger i = 0; i < argCount; ++i) {
    id arg;
    // +2 to ignore _sel and _cmd
    [invocation getArgument:&arg atIndex:i + 2];
    [args addObject:arg];
  }
  NSDictionary *error = nil;
  NSAppleEventDescriptor *desc = [self gtm_executePositionalHandler:handlerName 
                                                         parameters:args 
                                                              error:&error];  
  if ([[invocation methodSignature] methodReturnLength] > 0) {
    id returnValue = [desc gtm_objectValue];
    [invocation setReturnValue:&returnValue];
  }
}
@end

@implementation NSAppleEventDescriptor(GMAppleEventDescriptorScriptAdditions)

- (NSAppleScript*)gtm_scriptValue {
  NSDictionary *error;
  NSAppleScript *script = [[[NSAppleScript alloc] _initWithData:[self data] 
                                                          error:&error] autorelease];
  if (!script) {
    _GTMDevLog(@"Unable to create script: %@", error);  // COV_NF_LINE
  }
  return script;
}

- (NSString*)gtm_eventValue {
  struct AEEventRecordStruct {
    AEEventClass eventClass;
    AEEventID eventID;
  };
  NSData *data = [self data];
  const struct AEEventRecordStruct *record 
    = (const struct AEEventRecordStruct*)[data bytes];
  NSString *eClass = [GTMFourCharCode stringWithFourCharCode:record->eventClass];
  NSString *eID = [GTMFourCharCode stringWithFourCharCode:record->eventID];
  return [eClass stringByAppendingString:eID];
}
@end

