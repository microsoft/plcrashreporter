//
//  GTMXcodeCorrectWhitespace.m
//
//  Copyright 2007-2011 Google Inc.
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

#import "GTMXcodePluginManager.h"
#import <Cocoa/Cocoa.h>

@interface GTMXcodeCorrectWhitespace : GTMXcodePlugin
- (void)cleanUpWhitespace:(id)sender;
@end

@implementation GTMXcodeCorrectWhitespace

// Perform a "subtraction of ranges". A - B. Not transitive.
static NSRange SubtractRange(NSRange a, NSRange b) {
  NSRange newRange;
  NSUInteger maxRangeA = NSMaxRange(a);
  NSUInteger maxRangeB = NSMaxRange(b);
  if (b.location == NSNotFound) {
    // B is bogus
    newRange = a;
  } else if (maxRangeB <= a.location) {
    // B is completely before A
    newRange = NSMakeRange(a.location - b.length, a.length);
  } else if (maxRangeA <= b.location) {
    // B is completely after A
    newRange = a;
  } else if (b.location <= a.location && maxRangeB >= maxRangeA) {
    // B subsumes A
    newRange = NSMakeRange(b.location, 0);
  } else if (a.location <= b.location && maxRangeA >= maxRangeB) {
    // A subsumes B
    newRange = NSMakeRange(a.location, a.length - b.length);
  } else if (b.location <= a.location && maxRangeB <= maxRangeA) {
    // B overlaps front edge of A
    NSUInteger diff = maxRangeB - a.location;
    newRange = NSMakeRange(a.location + diff, a.length - diff);
  } else if (b.location <= maxRangeA && maxRangeB >= maxRangeA) {
    // B overlaps back edge of A
    NSUInteger diff = maxRangeA - b.location;
    newRange = NSMakeRange(a.location, a.length - diff);
  }
  return newRange;
}

+ (void)load {
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  [GTMXcodePluginManager registerPluginClass:self];
  [pool release];
}

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
  NSMenu *menu = [NSApp mainMenu];
  NSArray *items = [menu itemArray];
  for (NSMenuItem *item in items) {
    // Added to Edit instead of Editor menu because the Editor menu is
    // very dynamic in Xcode4.
    if ([[item title] isEqualToString:@"Edit"]) {
      NSMenu *editMenu = [item submenu];
      [editMenu addItem:[NSMenuItem separatorItem]];
      NSMenuItem *whiteSpaceItem
          = [[NSMenuItem alloc] initWithTitle:@"Clean Up Whitespace"
                                       action:@selector(cleanUpWhitespace:)
                                keyEquivalent:@""];
      [whiteSpaceItem setKeyEquivalent:@"s"];
      [whiteSpaceItem setKeyEquivalentModifierMask:(NSCommandKeyMask |
                                                    NSControlKeyMask |
                                                    NSAlternateKeyMask)];
      [editMenu addItem:whiteSpaceItem];
      [whiteSpaceItem setTarget:self];
    }
  }
}

- (void)cleanUpWhitespace:(id)sender {
  NSWindow *nsKeyWindow = [[NSApplication sharedApplication] keyWindow];
  NSResponder *responder = [nsKeyWindow firstResponder];
  if ([responder conformsToProtocol:@protocol(NSTextInputClient)]) {
    id client = responder;
    NSRange selectedRange = [client selectedRange];
    NSMutableAttributedString *src
        = [[[client attributedString] mutableCopy] autorelease];
    NSMutableString *text = [NSMutableString stringWithString:[src string]];

    // Figure out the newlines in our file.
    NSString *newlineString = @"\n";
    if ([text rangeOfString:@"\r\n"].length > 0) {
      newlineString = @"\r\n";
    } else if ([text rangeOfString:@"\r"].length > 0) {
      newlineString = @"\r";
    }
    NSUInteger newlineStringLength = [newlineString length];
    NSCharacterSet *whiteSpace
        = [NSCharacterSet characterSetWithCharactersInString:@" \t"];
    NSMutableCharacterSet *nonWhiteSpace
        = [[whiteSpace mutableCopy] autorelease];
    [nonWhiteSpace invert];

    NSRange oldRange = NSMakeRange(0, [text length]);
    NSRange textRange = oldRange;
    textRange.length -= 1;
    while (textRange.length > 0) {
      NSRange lineRange = [text rangeOfString:newlineString
                                      options:NSBackwardsSearch
                                        range:textRange];
      if (lineRange.location == NSNotFound) {
        lineRange.location = 0;
      } else {
        lineRange.location += newlineStringLength;
      }
      lineRange.length = textRange.length - lineRange.location;
      textRange.length = lineRange.location;
      if (textRange.length != 0) {
        textRange.length -= newlineStringLength;
      }

      NSRange whiteRange = [text rangeOfCharacterFromSet:whiteSpace
                                                 options:NSBackwardsSearch
                                                   range:lineRange];
      if (NSMaxRange(whiteRange) == NSMaxRange(lineRange)) {
        NSRange nonWhiteRange = [text rangeOfCharacterFromSet:nonWhiteSpace
                                                      options:NSBackwardsSearch
                                                        range:lineRange];
        NSRange deleteRange;
        if (nonWhiteRange.location == NSNotFound) {
          deleteRange.location = lineRange.location;
        } else {
          deleteRange.location = NSMaxRange(nonWhiteRange);
        }
        deleteRange.length = NSMaxRange(whiteRange) - deleteRange.location;
        [text deleteCharactersInRange:deleteRange];
        selectedRange = SubtractRange(selectedRange, deleteRange);
      }
    }

    // If the file is missing a newline at the end, add it now.
    if (![text hasSuffix:newlineString]) {
      [text appendString:newlineString];
    }

    [client insertText:text replacementRange:oldRange];
    if ([client respondsToSelector:@selector(setSelectedRange:)]) {
      if (NSMaxRange(selectedRange) < [text length]) {
        [client setSelectedRange:selectedRange];
        [client scrollRangeToVisible:selectedRange];
      }
    }
  }
}

@end
