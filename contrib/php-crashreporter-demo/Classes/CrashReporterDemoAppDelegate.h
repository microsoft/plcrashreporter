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

#import <UIKit/UIKit.h>
#import <CrashReporter/CrashReporter.h>
#import "StartupViewController.h"

@class CrashReporterDemoViewController;

@interface CrashReporterDemoAppDelegate : NSObject <UIApplicationDelegate> {
	UIApplication *_application;

	IBOutlet UIWindow *window;
	IBOutlet CrashReporterDemoViewController *viewController;
	
	IBOutlet StartupViewController *startupViewController;

	BOOL _autoSendCrashData;			// true if the user allowed the app to always send the crash data to the developers

@private
	
	NSData *_crashData;								// holds the last crash report	

	time_t _memoryWarningTimestamp;		// timestamp when memory warning appeared, we check on terminate if that timestamp is within a reasonable range to avoid false alarms
	BOOL _memoryWarningReached;				// true if memory warning notification is run at least once
	int _startupFreeMemory;						// amount of memory available at startup
	int _lastStartupFreeMemory;				// free memory at the last startup run

	int	_crashReportAnalyzerStarted;	// flags if the crashlog analyzer is started. since this may crash we need to track it
	int _shutdownFreeMemory;					// amount of memory available at shutdown
	int _lastShutdownFreeMemory;			// free memory at the last shutdown run
}

@property (nonatomic, retain) IBOutlet UIWindow *window;
@property (nonatomic, retain) IBOutlet CrashReporterDemoViewController *viewController;
@property (nonatomic, retain) IBOutlet StartupViewController *startupViewController;

- (int) getStartupFreeMemory;
- (int) getLastStartupFreeMemory;
- (int) getShutdownFreeMemory;
- (int) getLastShutdownFreeMemory;

- (void) finishCrashLog;

- (BOOL) isAutoSendCrashData;
- (void) setAutoSendCrashData:(BOOL)autoSend;
- (NSData *)getCrashData;
- (BOOL) isAllowContact;
- (NSString *) getContactEmail;

@end

