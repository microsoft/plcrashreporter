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
#import "HudView.h"

@interface StartupViewController : UIViewController <UIAlertViewDelegate> {
	IBOutlet UILabel *label;
	IBOutlet UIView *hudView;
	IBOutlet HudView *hudBox;
	IBOutlet UIActivityIndicatorView *loadingIndicator;
	
	UIAlertView *_alertView;
	
@private
	int _alertType;
	
	int _serverResult;

	BOOL _memoryCrash;
	
	BOOL _crashIdenticalCurrentVersion;
	
	NSString *_crashLogAppVersion;
	
	NSMutableString *_contentOfProperty;	
}

@property (nonatomic, retain) UILabel *label;
@property (nonatomic, retain) UIView *hudView;
@property (nonatomic, retain) HudView *hudBox;
@property (nonatomic, retain) UIActivityIndicatorView *loadingIndicator;

@property (nonatomic, retain) NSString *_crashLogAppVersion;

- (void) startCrashData;

- (void) showHUD:(NSString *)title;
- (void) removeHUD;

@end
