//
//  GTMNSAppleScript+Handler.h
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

#import <Foundation/Foundation.h>
#import "GTMDefines.h"

// A category for calling handlers in NSAppleScript
@interface NSAppleScript(GTMAppleScriptHandlerAdditions)
// This method allows us to call a specific handler in an AppleScript.
// parameters are passed in left-right order 0-n.
//
// Args:
//   handler - name of the handler to call in the Applescript
//   params - the parameters to pass to the handler
//   error - in non-nil returns any error that may have occurred.
//
// Returns:
//   The result of the handler being called. nil on failure.
- (NSAppleEventDescriptor*)gtm_executePositionalHandler:(NSString*)handler 
                                             parameters:(NSArray*)params 
                                                  error:(NSDictionary**)error;


- (NSAppleEventDescriptor*)gtm_executeLabeledHandler:(NSString*)handler
                                              labels:(AEKeyword*)labels
                                          parameters:(id*)params
                                               count:(NSUInteger)count
                                               error:(NSDictionary **)error;
- (NSSet*)gtm_handlers;
- (NSSet*)gtm_properties;
- (BOOL)gtm_setValue:(id)value forProperty:(NSString*)property;
- (id)gtm_valueForProperty:(NSString*)property;

@end

@interface NSAppleEventDescriptor(GTMAppleEventDescriptorScriptAdditions)

// Return an NSAppleScript for a desc of typeScript
// Returns nil on failure.
- (NSAppleScript*)gtm_scriptValue;

// Return a NSString with [eventClass][eventID] for typeEvent 'evnt'
- (NSString*)gtm_eventValue;
@end
