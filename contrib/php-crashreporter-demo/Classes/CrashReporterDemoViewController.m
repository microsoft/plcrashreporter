/*
 * Author: Andreas Linde <mail@andreaslinde.de>
 *
 * Copyright (c) 2009 Andreas Linde. All rights reserved.
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#import "CrashReporterDemoViewController.h"

@implementation CrashReporterDemoViewController

@synthesize triggerButtonMemoryLeak;
@synthesize triggerButtonCrash;


- (void) allocMoreMemory
{
	NSLog(@"Alloc");
	NSData *memory1 = [NSData dataWithContentsOfFile:[[[NSBundle mainBundle] resourcePath] stringByAppendingPathComponent:@"Default.png"]];
	[memory1 retain];
	NSData *memory2 = [NSData dataWithContentsOfFile:[[[NSBundle mainBundle] resourcePath] stringByAppendingPathComponent:@"Default.png"]];
	[memory2 retain];
}
 
 
- (IBAction) triggerMemoryLeak
{
	NSTimer *newTimer;
	newTimer = [NSTimer scheduledTimerWithTimeInterval: 0.1
																							target: self
																						selector: @selector(allocMoreMemory)
																						userInfo: nil
																						 repeats: YES];
}


- (IBAction) triggerCrash
{
	/* Trigger a crash */
	CFRelease(NULL);
}


- (void)didReceiveMemoryWarning {
    [super didReceiveMemoryWarning]; // Releases the view if it doesn't have a superview
    // Release anything that's not essential, such as cached data
}


- (void)dealloc {
    [super dealloc];
}

@end
