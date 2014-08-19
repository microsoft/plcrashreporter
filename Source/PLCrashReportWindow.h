#import <Cocoa/Cocoa.h>
#import <CrashReporter/CrashReporter.h>

extern NSString* const PLCrashWindowAutoSubmitKey; // if set the user's defaults is a BOOL, YES to send NO to cancel
extern NSString* const PLCrashWindowSubmitURLKey; // if set in the bundle's info dictionary the url to submit the crash report to, can be a mailto: url
extern NSString* const PLCrashWindowSubmitEmailKey; // if set the backup email for submissions, if the primary URL is http and the user declines to upload
extern NSString* const PLCrashWindowIncludeSyslogKey; // if set to YES then syslog messages with the applications bundle name in them are included
extern NSString* const PLCrashWindowIncludeDefaultsKey; // if set to YES then the applications preferences are included in the report

static NSString* const PLCrashWindowInsecureConnectionString = @"Insure Connection";
static NSString* const PLCrashWIndowInsecureConnectionInformationString = @"%@ does not support secure crash reporting, your crash report will be sent in plaintext and may be observed while in transit."; // app name
static NSString* const PLCrashWindowInsecureConnectionEmailAlternateString = @"\n\nEmail may be a more secure option, depending on your provider.";
static NSString* const PLCrashWindowCancelString = @"Cancel";
static NSString* const PLCrashWindowSendString = @"Send";
static NSString* const PLCrashWindowEmailString = @"Email";
static NSString* const PLCrashWindowCrashReportString = @"Crash Report";
static NSString* const PLCrashWindowExceptionReportString = @"Exception Report";
static NSString* const PLCrashWindowErrorReportString = @"Error Report";
static NSString* const PLCrashWindowCrashedString = @"Crashed!";
static NSString* const PLCrashWindowRaisedExceptionString = @"Raised an Exception";
static NSString* const PLCrashWindowReportedErrorString = @"Reported an Error";
static NSString* const PLCrashWindowCrashDispositionString = @"Click Report to send the report to the devleoper, or Cancel to ignore it.";
static NSString* const PLCrashWindowErrorDispositionString = @"Click Restart to send the report to the devloper and restart the application, or Quit.";
static NSString* const PLCrashWindowReportString = @"Report";
static NSString* const PLCrashWindowRestartString = @"Restart";
static NSString* const PLCrashWindowQuitString = @"Quit";
static NSString* const PLCrashWindowCommentsString = @"please enter any comments here";
static NSString* const PLCrashWindowSubmitFailedString = @"Submitting Report Failed";
static NSString* const PLCrashWindowSubmitFailedInformationString = @"%@ was not able to submit the report to: %@\n\nyou can send the report by email"; // app name and submission url

@interface PLCrashReportWindow : NSWindowController <NSURLConnectionDelegate>
{
    NSError* error;
    NSException* exception;
    PLCrashReporter* reporter;
    NSData* crashData;
    PLCrashReport* crashReport;
    NSHTTPURLResponse* response;
    NSMutableData* responseBody;
    NSModalSession modalSession;
    NSTextField* headline;
    NSTextField* subhead;
    NSTextView* comments;
    NSButton* remember;
    NSTextField* status;
    NSProgressIndicator* progress;
    NSButton* cancel;
    NSButton* send;
}
@property(nonatomic,retain) NSError* error;
@property(nonatomic,retain) NSException* exception;
@property(nonatomic,retain) PLCrashReporter* reporter;
@property(nonatomic,retain) NSData* crashData;
@property(nonatomic,retain) PLCrashReport* crashReport;
@property(nonatomic,retain) NSHTTPURLResponse* response;
@property(nonatomic,retain) NSMutableData* responseBody;
@property(nonatomic,assign) NSModalSession modalSession;
@property(nonatomic,retain) IBOutlet NSTextField* headline;
@property(nonatomic,retain) IBOutlet NSTextField* subhead;
@property(nonatomic,retain) IBOutlet NSTextView* comments;
@property(nonatomic,retain) IBOutlet NSButton* remember;
@property(nonatomic,retain) IBOutlet NSTextField* status;
@property(nonatomic,retain) IBOutlet NSProgressIndicator* progress;
@property(nonatomic,retain) IBOutlet NSButton* cancel;
@property(nonatomic,retain) IBOutlet NSButton* send;

+ (instancetype) windowForReporter:(PLCrashReporter*) reporter;
+ (instancetype) windowForReporter:(PLCrashReporter*) reporter withError:(NSError*) error;
+ (instancetype) windowForReporter:(PLCrashReporter*) reporter withException:(NSException*) exception;

- (void) runModal;

#pragma mark - IBActions

- (IBAction)onCancel:(id)sender;
- (IBAction)onSend:(id)sender;

@end
