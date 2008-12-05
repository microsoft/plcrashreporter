//
//  GTMNSObject+BindingUnitTesting.m
//  
//  An informal protocol for doing advanced binding unittesting with objects.
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

#import "GTMDefines.h"
#import "GTMNSObject+BindingUnitTesting.h"

BOOL GTMDoExposedBindingsFunctionCorrectly(NSObject *object, 
                                           NSArray **errors) {
  NSMutableArray *errorArray = [NSMutableArray array];
  if (errors) {
    *errors = nil;
  }
  NSArray *bindings = [object exposedBindings];
  if ([bindings count]) {
    NSArray *bindingsToIgnore = [object gtm_unitTestExposedBindingsToIgnore];
    NSEnumerator *bindingsEnum = [bindings objectEnumerator];
    NSString *bindingKey;
    while ((bindingKey = [bindingsEnum nextObject])) {
      if (![bindingsToIgnore containsObject:bindingKey]) {
        Class theClass = [object valueClassForBinding:bindingKey];
        if (!theClass) {
          [errorArray addObject:[NSString stringWithFormat:@"%@ should have valueClassForBinding '%@'",
                                 object, bindingKey]];
          continue;
        }
        @try {
          @try {
            [object valueForKey:bindingKey];
          }
          @catch (NSException *e) {
            _GTMDevLog(@"%@ is not key value coding compliant for key %@", object, bindingKey);
            continue;
          }  // COV_NF_LINE - compiler bug
          NSDictionary *testValues = [object gtm_unitTestExposedBindingsTestValues:bindingKey];
          NSEnumerator *testEnum = [testValues keyEnumerator];
          id testValue;
          while ((testValue = [testEnum nextObject])) {
            [object setValue:testValue forKey:bindingKey];
            id value = [object valueForKey:bindingKey];
            id desiredValue = [testValues objectForKey:testValue];
            if (![desiredValue gtm_unitTestIsEqualTo:value]) {
              [errorArray addObject:[NSString stringWithFormat:@"%@ unequal to %@ for binding '%@'",
                                     value, desiredValue, bindingKey]];
              continue;
            }
          }
        }
        @catch(NSException *e) {
          [errorArray addObject:[NSString stringWithFormat:@"%@:%@-> Binding %@", 
                                 [e name], [e reason], bindingKey]];
        }  // COV_NF_LINE - compiler bug
      }
    }
  } else {
    [errorArray addObject:[NSString stringWithFormat:@"%@ does not have any exposed bindings",
                           object]];
  }
  if (errors) {
    *errors = errorArray;
  }
  return [errorArray count] == 0;
}

// Utility for simplifying unitTestExposedBindingsTestValues implementations
@interface NSMutableDictionary (GTMUnitTestingAdditions)
// Sets an object and a key to the same value in a dictionary.
- (void)gtm_setObjectAndKey:(id)objectAndKey;
@end

@implementation NSObject (GTMBindingUnitTestingAdditions)

- (NSMutableArray*)gtm_unitTestExposedBindingsToIgnore {
  NSMutableArray *array = [NSMutableArray arrayWithObject:NSValueBinding];
  if ([[self exposedBindings] containsObject:NSFontBinding]) {
    NSString *fontBindings[] = { NSFontBoldBinding, NSFontFamilyNameBinding, 
    NSFontItalicBinding, NSFontNameBinding, NSFontSizeBinding };
    for (size_t i = 0; i < sizeof(fontBindings) / sizeof(NSString*); ++i) {
      [array addObject:fontBindings[i]];
    }
  }
  return array;
}

- (NSMutableDictionary*)gtm_unitTestExposedBindingsTestValues:(NSString*)binding {
  
  NSMutableDictionary *dict = [NSMutableDictionary dictionary];
  id value = [self valueForKey:binding];
  
  // Always test identity if possible
  if (value) {
    [dict gtm_setObjectAndKey:value];
  }
  
  // Now some default test values for a variety of bindings to make
  // sure that we cover all the bases and save other people writing lots of
  // duplicate test code.
  
  // If anybody can think of more to add, please go nuts.
  if ([binding isEqualToString:NSAlignmentBinding]) {
    [dict gtm_setObjectAndKey:[NSNumber numberWithInt:NSLeftTextAlignment]];
    [dict gtm_setObjectAndKey:[NSNumber numberWithInt:NSRightTextAlignment]];
    [dict gtm_setObjectAndKey:[NSNumber numberWithInt:NSCenterTextAlignment]];
    [dict gtm_setObjectAndKey:[NSNumber numberWithInt:NSJustifiedTextAlignment]];
    NSNumber *natural = [NSNumber numberWithInt:NSNaturalTextAlignment];
    [dict gtm_setObjectAndKey:natural];
    [dict setObject:natural forKey:[NSNumber numberWithInt:500]];
    [dict setObject:natural forKey:[NSNumber numberWithInt:-1]];
  } else if ([binding isEqualToString:NSAlternateImageBinding] || 
             [binding isEqualToString:NSImageBinding] || 
             [binding isEqualToString:NSMixedStateImageBinding] || 
             [binding isEqualToString:NSOffStateImageBinding] ||
             [binding isEqualToString:NSOnStateImageBinding]) {
    // This handles all image bindings
    [dict gtm_setObjectAndKey:[NSImage imageNamed:@"NSApplicationIcon"]];
  } else if ([binding isEqualToString:NSAnimateBinding] || 
             [binding isEqualToString:NSDocumentEditedBinding] ||
             [binding isEqualToString:NSEditableBinding] ||
             [binding isEqualToString:NSEnabledBinding] ||
             [binding isEqualToString:NSHiddenBinding] ||
             [binding isEqualToString:NSVisibleBinding] || 
             [binding isEqualToString:NSIsIndeterminateBinding] ||
             // NSTranparentBinding 10.5 only
             [binding isEqualToString:@"transparent"]) {
    // This handles all bool value bindings
    [dict gtm_setObjectAndKey:[NSNumber numberWithBool:YES]];
    [dict gtm_setObjectAndKey:[NSNumber numberWithBool:NO]];
  } else if ([binding isEqualToString:NSAlternateTitleBinding] ||
             [binding isEqualToString:NSHeaderTitleBinding] ||
             [binding isEqualToString:NSLabelBinding] ||
             [binding isEqualToString:NSTitleBinding] ||
             [binding isEqualToString:NSToolTipBinding]) {
    // This handles all string value bindings
    [dict gtm_setObjectAndKey:@"happy"];
    [dict gtm_setObjectAndKey:@""];
    
    // Test some non-ascii roman text
    char a_not_alpha[] = { 'A', 0xE2, 0x89, 0xA2, 0xCE, 0x91, '.', 0x00 };
    [dict gtm_setObjectAndKey:[NSString stringWithUTF8String:a_not_alpha]];
    // Test some korean
    char hangugo[] = { 0xED, 0x95, 0x9C, 0xEA, 0xB5, 
                       0xAD, 0xEC, 0x96, 0xB4, 0x00 };    
    [dict gtm_setObjectAndKey:[NSString stringWithUTF8String:hangugo]];
    // Test some japanese
    char nihongo[] = { 0xE6, 0x97, 0xA5, 0xE6, 0x9C,
                       0xAC, 0xE8, 0xAA, 0x9E, 0x00 };
    [dict gtm_setObjectAndKey:[NSString stringWithUTF8String:nihongo]];
    // Test some arabic 
    char arabic[] = { 0xd9, 0x83, 0xd8, 0xa7, 0xd9, 0x83, 0xd8, 0xa7, 0x00 };
    [dict gtm_setObjectAndKey:[NSString stringWithUTF8String:arabic]];
  } else if ([binding isEqualToString:NSRepresentedFilenameBinding]) {
    // This handles all path bindings
    [dict gtm_setObjectAndKey:@"/happy"];
    [dict gtm_setObjectAndKey:@"/"];
    
    // Test some non-ascii roman text
    char a_not_alpha[] = { '/', 'A', 0xE2, 0x89, 0xA2, 0xCE, 0x91, '.', 0x00 };
    [dict gtm_setObjectAndKey:[NSString stringWithUTF8String:a_not_alpha]];
    // Test some korean
    char hangugo[] = { '/', 0xED, 0x95, 0x9C, 0xEA, 0xB5, 
    0xAD, 0xEC, 0x96, 0xB4, 0x00 };    
    [dict gtm_setObjectAndKey:[NSString stringWithUTF8String:hangugo]];
    // Test some japanese
    char nihongo[] = { '/', 0xE6, 0x97, 0xA5, 0xE6, 0x9C,
    0xAC, 0xE8, 0xAA, 0x9E, 0x00 };
    [dict gtm_setObjectAndKey:[NSString stringWithUTF8String:nihongo]];
    // Test some arabic 
    char arabic[] = { '/', 0xd9, 0x83, 0xd8, 0xa7, 0xd9, 0x83, 0xd8, 0xa7, 0x00 };
    [dict gtm_setObjectAndKey:[NSString stringWithUTF8String:arabic]];
  } else if ([binding isEqualToString:NSMaximumRecentsBinding] ||
             [binding isEqualToString:NSRowHeightBinding]) {
    // This handles all int value bindings
    [dict gtm_setObjectAndKey:[NSNumber numberWithInt:0]];
    [dict gtm_setObjectAndKey:[NSNumber numberWithInt:-1]];
    [dict gtm_setObjectAndKey:[NSNumber numberWithInt:INT16_MAX]];
    [dict gtm_setObjectAndKey:[NSNumber numberWithInt:INT16_MIN]];
  } else if ([binding isEqualToString:NSMaxValueBinding] ||
             [binding isEqualToString:NSMaxWidthBinding] ||
             [binding isEqualToString:NSMinValueBinding] ||
             [binding isEqualToString:NSMinWidthBinding] || 
             [binding isEqualToString:NSContentWidthBinding] || 
             [binding isEqualToString:NSContentHeightBinding] ||
             [binding isEqualToString:NSWidthBinding] ||
             [binding isEqualToString:NSAnimationDelayBinding]) {
    // This handles all float value bindings
    [dict gtm_setObjectAndKey:[NSNumber numberWithFloat:0]];
    [dict gtm_setObjectAndKey:[NSNumber numberWithFloat:FLT_MAX]];
    [dict gtm_setObjectAndKey:[NSNumber numberWithFloat:-FLT_MAX]];
    [dict gtm_setObjectAndKey:[NSNumber numberWithFloat:FLT_MIN]];
    [dict gtm_setObjectAndKey:[NSNumber numberWithFloat:-FLT_MIN]];
    [dict gtm_setObjectAndKey:[NSNumber numberWithFloat:FLT_EPSILON]];
    [dict gtm_setObjectAndKey:[NSNumber numberWithFloat:-FLT_EPSILON]];
  } else if ([binding isEqualToString:NSTextColorBinding]) {
    // This handles all color value bindings
    [dict gtm_setObjectAndKey:[NSColor colorWithCalibratedWhite:1.0 alpha:1.0]];
    [dict gtm_setObjectAndKey:[NSColor colorWithCalibratedWhite:1.0 alpha:0.0]];
    [dict gtm_setObjectAndKey:[NSColor colorWithCalibratedWhite:1.0 alpha:0.5]];
    [dict gtm_setObjectAndKey:[NSColor colorWithCalibratedRed:0.5 green:0.5 
                                                         blue:0.5 alpha:0.5]];
    [dict gtm_setObjectAndKey:[NSColor colorWithDeviceCyan:0.25 magenta:0.25 
                                                    yellow:0.25 black:0.25 
                                                     alpha:0.25]];
  } else if ([binding isEqualToString:NSFontBinding]) {
    // This handles all font value bindings
    [dict gtm_setObjectAndKey:[NSFont boldSystemFontOfSize:[NSFont systemFontSize]]];
    [dict gtm_setObjectAndKey:[NSFont toolTipsFontOfSize:[NSFont smallSystemFontSize]]];
    [dict gtm_setObjectAndKey:[NSFont labelFontOfSize:144.0]];
  } else if ([binding isEqualToString:NSRecentSearchesBinding] || 
             [binding isEqualToString:NSSortDescriptorsBinding]) {
    // This handles all array value bindings
    [dict gtm_setObjectAndKey:[NSArray array]];
  } else if ([binding isEqualToString:NSTargetBinding]) {
    [dict gtm_setObjectAndKey:[NSNull null]];
  } else {
    _GTMDevLog(@"Skipped Binding: %@ for %@", binding, self);  // COV_NF_LINE
  }
  return dict;
}

- (BOOL)gtm_unitTestIsEqualTo:(id)value {
  return [self isEqualTo:value];
}

@end

@implementation NSMutableDictionary (GTMUnitTestingAdditions)
// Sets an object and a key to the same value in a dictionary.
- (void)gtm_setObjectAndKey:(id)objectAndKey {
  [self setObject:objectAndKey forKey:objectAndKey];
}
@end

#pragma mark -
#pragma mark All the special AppKit Bindings issues below

@interface NSImage (GTMBindingUnitTestingAdditions) 
@end

@implementation NSImage (GTMBindingUnitTestingAdditions)
- (BOOL)gtm_unitTestIsEqualTo:(id)value {
  // NSImage just does pointer equality in the default isEqualTo implementation
  // we need something a little more heavy duty that actually compares the
  // images internally.
  return [[self TIFFRepresentation] isEqualTo:[value TIFFRepresentation]];
}
@end

@interface NSScroller (GTMBindingUnitTestingAdditions) 
@end

@implementation NSScroller (GTMBindingUnitTestingAdditions)
- (NSMutableArray*)gtm_unitTestExposedBindingsToIgnore {
  NSMutableArray *array = [super gtm_unitTestExposedBindingsToIgnore];
  // rdar://5849154 - NSScroller exposes binding with no value class for NSValueBinding
  [array addObject:NSValueBinding];
  // rdar://5849236 - NSScroller exposes binding for NSFontBinding
  [array addObject:NSFontBinding];
  return array;
}
@end

@interface NSTextField (GTMBindingUnitTestingAdditions) 
@end

@implementation NSTextField (GTMBindingUnitTestingAdditions)

- (NSMutableArray*)gtm_unitTestExposedBindingsToIgnore {
  NSMutableArray *array = [super gtm_unitTestExposedBindingsToIgnore];
  // Not KVC Compliant
  for (int i = 0; i < 10; i++) {
    [array addObject:[NSString stringWithFormat:@"displayPatternValue%d", i]];
  }
  return array;
}

- (NSMutableDictionary*)gtm_unitTestExposedBindingsTestValues:(NSString*)binding {
  NSMutableDictionary *dict = [super gtm_unitTestExposedBindingsTestValues:binding];
  if ([binding isEqualToString:NSAlignmentBinding]) {
    // rdar://5851487 - If NSAlignmentBinding for a NSTextField is set to -1 and then got it returns 7
    [dict setObject:[NSNumber numberWithInt:7] forKey:[NSNumber numberWithInt:-1]];
  }
  return dict;
}
@end

@interface NSSearchField (GTMBindingUnitTestingAdditions) 
@end

@implementation NSSearchField (GTMBindingUnitTestingAdditions)

- (NSMutableDictionary*)gtm_unitTestExposedBindingsTestValues:(NSString*)binding {
  NSMutableDictionary *dict = [super gtm_unitTestExposedBindingsTestValues:binding];
#if !__LP64__
  if ([binding isEqualToString:NSAlignmentBinding]) {
    // rdar://5851491 - Setting NSAlignmentBinding of search field to NSCenterTextAlignment broken
    // This appears to not be a bug in 64 bit. Strange.
    [dict setObject:[NSNumber numberWithInt:NSNaturalTextAlignment] 
             forKey:[NSNumber numberWithInt:NSCenterTextAlignment]];
  }
#endif
  return dict;
}

- (NSMutableArray*)gtm_unitTestExposedBindingsToIgnore {
  NSMutableArray *array = [super gtm_unitTestExposedBindingsToIgnore];
  // Not KVC Compliant
  [array addObject:NSPredicateBinding];
  return array;
}

@end

@interface NSWindow (GTMBindingUnitTestingAdditions) 
@end

@implementation NSWindow (GTMBindingUnitTestingAdditions)

- (NSMutableArray*)gtm_unitTestExposedBindingsToIgnore {
  NSMutableArray *array = [super gtm_unitTestExposedBindingsToIgnore];
  // Not KVC Compliant
  [array addObject:NSContentWidthBinding];
  [array addObject:NSContentHeightBinding];
  for (int i = 0; i < 10; i++) {
    [array addObject:[NSString stringWithFormat:@"displayPatternTitle%d", i]];
  }
  return array;
}

@end

@interface NSBox (GTMBindingUnitTestingAdditions) 
@end

@implementation NSBox (GTMBindingUnitTestingAdditions)

- (NSMutableArray*)gtm_unitTestExposedBindingsToIgnore {
  NSMutableArray *array = [super gtm_unitTestExposedBindingsToIgnore];
  // Not KVC Compliant
  for (int i = 0; i < 10; i++) {
    [array addObject:[NSString stringWithFormat:@"displayPatternTitle%d", i]];
  }
  return array;
}

@end

@interface NSTableView (GTMBindingUnitTestingAdditions) 
@end

@implementation NSTableView (GTMBindingUnitTestingAdditions)

- (NSMutableArray*)gtm_unitTestExposedBindingsToIgnore {
  NSMutableArray *array = [super gtm_unitTestExposedBindingsToIgnore];
  // rdar://5849684 - NSTableView should throw exception when attempting to set NSFontBinding
  [array addObject:NSFontBinding];
  // Not KVC Compliant
  [array addObject:NSContentBinding];
  [array addObject:NSDoubleClickTargetBinding];
  [array addObject:NSDoubleClickArgumentBinding];
  [array addObject:NSSelectionIndexesBinding];
  return array;
}

@end

@interface NSTextView (GTMBindingUnitTestingAdditions) 
@end

@implementation NSTextView (GTMBindingUnitTestingAdditions)

- (NSMutableArray*)gtm_unitTestExposedBindingsToIgnore {
  NSMutableArray *array = [super gtm_unitTestExposedBindingsToIgnore];
  //rdar://5849335 - NSTextView only partially KVC compliant for key NSAttributedStringBinding
  [array addObject:NSAttributedStringBinding];
  // Not KVC Compliant
  [array addObject:NSDataBinding];
  [array addObject:NSValueURLBinding];
  [array addObject:NSValuePathBinding];
  return array;
}

@end

@interface NSTabView (GTMBindingUnitTestingAdditions) 
@end

@implementation NSTabView (GTMBindingUnitTestingAdditions)

- (NSMutableArray*)gtm_unitTestExposedBindingsToIgnore {
  NSMutableArray *array = [super gtm_unitTestExposedBindingsToIgnore];
  // rdar://5849248 - NSTabView exposes binding with no value class for NSSelectedIdentifierBinding 
  [array addObject:NSSelectedIdentifierBinding];
  // Not KVC Compliant
  [array addObject:NSSelectedIndexBinding];
  [array addObject:NSSelectedLabelBinding];
  return array;
}

@end

@interface NSButton (GTMBindingUnitTestingAdditions) 
@end

@implementation NSButton (GTMBindingUnitTestingAdditions)

- (NSMutableArray*)gtm_unitTestExposedBindingsToIgnore {
  NSMutableArray *array = [super gtm_unitTestExposedBindingsToIgnore];
  // Not KVC Compliant
  [array addObject:NSArgumentBinding];
  return array;
}

@end

@interface NSProgressIndicator (GTMBindingUnitTestingAdditions) 
@end

@implementation NSProgressIndicator (GTMBindingUnitTestingAdditions)

- (NSMutableArray*)gtm_unitTestExposedBindingsToIgnore {
  NSMutableArray *array = [super gtm_unitTestExposedBindingsToIgnore];
  // Not KVC Compliant
  [array addObject:NSAnimateBinding];
  return array;
}

@end
