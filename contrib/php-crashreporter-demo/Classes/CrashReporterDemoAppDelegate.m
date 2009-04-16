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

#import "CrashReporterDemoAppDelegate.h"
#import "CrashReporterDemoViewController.h"
#import <mach/mach.h>
#import <mach/mach_host.h>

NSString *kAutoSendCrashDataKey = @"AutoSendCrashDataKey";								// preference key to check if the user allowed the application to send crash data always
NSString *kAllowBookmarkAccessKey = @"AllowBookmarkAccessKey";						// grant access to bookmarks
NSString *kCrashDataContactAllowKey = @"CrashDataContactAllowKey";				// allow to contact the user via email
NSString *kCrashDataContactEmailKey = @"CrashDataContactEmailKey";				// the users email address

NSString *kCrashReportAnalyzerStarted = @"CrashReportAnalyzerStarted";		// flags if the crashlog analyzer is started. since this may crash we need to track it
NSString *kLastRunMemoryWarningReached = @"LastRunMemoryWarningReached";	// is the last crash because of lowmemory warning?
NSString *kLastStartupFreeMemory = @"LastStartupFreeMemory";							// the amount of memory available on startup on the run of the app the crash happened
NSString *kLastShutdownFreeMemory = @"LastShutdownFreeMemory";						// the amount of memory available on shutdown on the run of the app the crash happened

@interface CrashReporterDemoAppDelegate ()
- (void)handleCrashReport;
- (void)sendCrashReport;

- (void)initViews;
- (void) getMemory:(BOOL)startup;
@end

@implementation CrashReporterDemoAppDelegate

@synthesize window;
@synthesize viewController;
@synthesize startupViewController;

- (void) getMemory:(BOOL)startup
{
	mach_port_t host_port;
	mach_msg_type_number_t host_size;
	vm_size_t pagesize;
	
	host_port = mach_host_self();
	host_size = sizeof(vm_statistics_data_t) / sizeof(integer_t);
	host_page_size(host_port, &pagesize);        
	
	vm_statistics_data_t vm_stat;
	
	if (host_statistics(host_port, HOST_VM_INFO, (host_info_t)&vm_stat, &host_size) != KERN_SUCCESS)
		NSLog(@"Failed to fetch vm statistics");
	
	natural_t mem_free = vm_stat.free_count * pagesize;
	if (startup)
		_startupFreeMemory = (mem_free / 1024 );
	else
		_shutdownFreeMemory = (mem_free / 1024 );
}


- (void)applicationDidFinishLaunching:(UIApplication *)application
{
	_application = application;
	
	[self getMemory:YES];
	
	_memoryWarningTimestamp = 0;
	
	NSString *testValue = [[NSUserDefaults standardUserDefaults] stringForKey:kAutoSendCrashDataKey];
	if (testValue == nil)
	{
		_autoSendCrashData = NO;		
	} else {
		_autoSendCrashData = [[NSUserDefaults standardUserDefaults] boolForKey:kAutoSendCrashDataKey];
	}
	
	
	testValue = nil;
	testValue = [[NSUserDefaults standardUserDefaults] stringForKey:kLastRunMemoryWarningReached];
	if (testValue == nil)
	{
		_memoryWarningReached = NO;		
	} else {
		_memoryWarningReached = [[NSNumber numberWithInt:[[NSUserDefaults standardUserDefaults] integerForKey:kLastRunMemoryWarningReached]] boolValue];
	}
	
	testValue = nil;
	testValue = [[NSUserDefaults standardUserDefaults] stringForKey:kCrashReportAnalyzerStarted];
	if (testValue == nil)
	{
		_crashReportAnalyzerStarted = 0;		
	} else {
		_crashReportAnalyzerStarted = [[NSNumber numberWithInt:[[NSUserDefaults standardUserDefaults] integerForKey:kCrashReportAnalyzerStarted]] intValue];
	}
	
	testValue = nil;
	testValue = [[NSUserDefaults standardUserDefaults] stringForKey:kLastStartupFreeMemory];
	if (testValue == nil)
	{
		_lastStartupFreeMemory = 0;		
	} else {
		_lastStartupFreeMemory = [[NSNumber numberWithInt:[[NSUserDefaults standardUserDefaults] integerForKey:kLastStartupFreeMemory]] intValue];
	}
	
	testValue = nil;
	testValue = [[NSUserDefaults standardUserDefaults] stringForKey:kLastShutdownFreeMemory];
	if (testValue == nil)
	{
		_lastShutdownFreeMemory = 0;		
	} else {
		_lastShutdownFreeMemory = [[NSNumber numberWithInt:[[NSUserDefaults standardUserDefaults] integerForKey:kLastShutdownFreeMemory]] intValue];
	}
	
	_crashData = nil;
	PLCrashReporter *crashReporter = [PLCrashReporter sharedReporter];
	NSError *error;
	
	// Check if we previously crashed
	if ([crashReporter hasPendingCrashReport])
		[self handleCrashReport];
	
	// Enable the Crash Reporter
	if (![crashReporter enableCrashReporterAndReturnError: &error])
		NSLog(@"Warning: Could not enable crash reporter: %@", error);
	
	if (_crashData != nil || ((_memoryWarningReached && _lastStartupFreeMemory != 0 && _lastShutdownFreeMemory < 2000)))
	{
		[self sendCrashReport];
	} else {
		[self finishCrashLog];
	}	
}


- (void)dealloc {
	[viewController release];
	[window release];
	[super dealloc];
}


- (void)applicationDidReceiveMemoryWarning:(UIApplication *)application
{
	_memoryWarningTimestamp = time(0);
}


// Save all changes to the database, then close it.
- (void)applicationWillTerminate:(UIApplication *)application
{
	[self getMemory:NO];
	
	time_t currentTime = time(0);
	
	// only assume a memory problem caused shutdown if the last memory warning timestamp was less than 3 seconds ago
	// DEACTIVATED since that didn't proove to be reliable enough
	/*
	if (abs(currentTime) - abs(_memoryWarningTimestamp) < 3)
	{
		_memoryWarningReached = YES;
	} else {
		_memoryWarningReached = NO;
	}
	 */
	
	
	// save current memory
	[[NSUserDefaults standardUserDefaults] setObject:[NSNumber numberWithInt:_memoryWarningReached] forKey:kLastRunMemoryWarningReached];
	[[NSUserDefaults standardUserDefaults] setObject:[NSNumber numberWithInt:_startupFreeMemory] forKey:kLastStartupFreeMemory];
	[[NSUserDefaults standardUserDefaults] setObject:[NSNumber numberWithInt:_shutdownFreeMemory] forKey:kLastShutdownFreeMemory];
}


- (void)initViews
{
	_memoryWarningReached = NO;
	
	// Override point for customization after app launch    
	[window addSubview:viewController.view];
	[window makeKeyAndVisible];
}


// this functions sends the crash data to the server
- (void)sendCrashReport
{
	[window addSubview:startupViewController.view];
	[startupViewController startCrashData];
}


- (void) finishCrashLog
{
	[startupViewController.view removeFromSuperview];
	[self initViews];
}


- (int) getStartupFreeMemory
{
	return _startupFreeMemory;
}


- (int) getLastStartupFreeMemory
{
	return _lastStartupFreeMemory;
}


- (int) getShutdownFreeMemory
{
	return _shutdownFreeMemory;
}


- (int) getLastShutdownFreeMemory
{
	return _lastShutdownFreeMemory;
}


- (BOOL) isAutoSendCrashData
{
	return _autoSendCrashData;
}


- (void) setAutoSendCrashData:(BOOL)autoSend
{
	_autoSendCrashData = autoSend;
	[[NSUserDefaults standardUserDefaults] setValue:[NSNumber numberWithBool:autoSend] forKey:kAutoSendCrashDataKey];
}


- (NSData *)getCrashData
{
	return _crashData;
}


- (BOOL) isAllowContact
{
	return [[NSUserDefaults standardUserDefaults] boolForKey:kCrashDataContactAllowKey];	
}


- (NSString *) getContactEmail
{
	return [[NSUserDefaults standardUserDefaults] stringForKey:kCrashDataContactEmailKey];
}


#pragma mark PLCrashReporter

//
// Called to handle a pending crash report.
//
- (void) handleCrashReport
{
	PLCrashReporter *crashReporter = [PLCrashReporter sharedReporter];
	NSError *error;
	
	// Try loading the crash report
	_crashData = [NSData dataWithData:[crashReporter loadPendingCrashReportDataAndReturnError: &error]];
	if (_crashData == nil) {
		NSLog(@"Could not load crash report: %@", error);
		goto finish;
	}
	
	// check if the next call ran successfully the last time
	if (_crashReportAnalyzerStarted == 0)
	{
		// mark the start of the routine
		_crashReportAnalyzerStarted = 1;
		[[NSUserDefaults standardUserDefaults] setValue:[NSNumber numberWithInt:_crashReportAnalyzerStarted] forKey:kCrashReportAnalyzerStarted];

		// We could send the report from here, but we'll just print out
		// some debugging info instead
		PLCrashReport *report = [[[PLCrashReport alloc] initWithData: [_crashData retain] error: &error] autorelease];
		if (report == nil) {
			NSLog(@"Could not parse crash report");
			goto finish;
		}
		
		NSLog(@"Crashed on %@", report.systemInfo.timestamp);
		NSLog(@"Crashed with signal %@ (code %@, address=0x%" PRIx64 ")", report.signalInfo.name,
						report.signalInfo.code, report.signalInfo.address);
	}
		
	// Purge the report
finish:
	// mark the end of the routine
	_crashReportAnalyzerStarted = 0;
	[[NSUserDefaults standardUserDefaults] setValue:[NSNumber numberWithInt:_crashReportAnalyzerStarted] forKey:kCrashReportAnalyzerStarted];
	
	[crashReporter purgePendingCrashReport];
	return;
}

@end