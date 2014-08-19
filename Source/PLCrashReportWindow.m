#import "PLCrashReportWindow.h"
#import "Mail.h"

#import <execinfo.h>
#import <ExceptionHandling/ExceptionHandling.h>

NSString* const PLCrashWindowAutoSubmitKey = @"PLCrashWindowAutoSubmitKey";
NSString* const PLCrashWindowSubmitURLKey = @"PLCrashWindowSubmitURLKey";
NSString* const PLCrashWindowSubmitEmailKey = @"PLCrashWindowSubmitEmailKey";
NSString* const PLCrashWindowIncludeSyslogKey = @"PLCrashWindowIncludeSyslogKey";
NSString* const PLCrashWindowIncludeDefaultsKey = @"PLCrashWindowIncludeDefaultsKey";

NSString* const PLCrashWindowInsecureConnectionString = @"PLCrashWindowInsecureConnectionString";
NSString* const PLCrashWIndowInsecureConnectionInformationString = @"PLCrashWIndowInsecureConnectionInformationString";
NSString* const PLCrashWindowInsecureConnectionEmailAlternateString = @"PLCrashWindowInsecureConnectionEmailAlternateString";
NSString* const PLCrashWindowCancelString = @"PLCrashWindowCancelString";
NSString* const PLCrashWindowSendString = @"PLCrashWindowSendString";
NSString* const PLCrashWindowEmailString = @"PLCrashWindowEmailString";
NSString* const PLCrashWindowCrashReportString = @"PLCrashWindowCrashReportString";
NSString* const PLCrashWindowExceptionReportString = @"PLCrashWindowExceptionReportString";
NSString* const PLCrashWindowErrorReportString = @"PLCrashWindowErrorReportString";
NSString* const PLCrashWindowCrashedString = @"PLCrashWindowCrashedString";
NSString* const PLCrashWindowRaisedExceptionString = @"PLCrashWindowRaisedExceptionString";
NSString* const PLCrashWindowReportedErrorString = @"PLCrashWindowReportedErrorString";
NSString* const PLCrashWindowCrashDispositionString = @"PLCrashWindowCrashDispositionString";
NSString* const PLCrashWindowErrorDispositionString = @"PLCrashWindowErrorDispositionString";
NSString* const PLCrashWindowReportString = @"PLCrashWindowReportString";
NSString* const PLCrashWindowRestartString = @"PLCrashWindowRestartString";
NSString* const PLCrashWindowQuitString = @"PLCrashWindowQuitString";
NSString* const PLCrashWindowCommentsString = @"PLCrashWindowCommentsString";
NSString* const PLCrashWindowSubmitFailedString = @"PLCrashWindowSubmitFailedString";
NSString* const PLCrashWindowSubmitFailedInformationString = @"PLCrashWindowSubmitFailedInformationString";

#define PLLocalizedString(key) [[NSBundle bundleForClass:[self class]] localizedStringForKey:(key) value:@"" table:[self className]]

#pragma mark -

@implementation PLCrashReportWindow
@synthesize error;
@synthesize exception;
@synthesize reporter;
@synthesize crashData;
@synthesize crashReport;
@synthesize response;
@synthesize responseBody;
@synthesize modalSession;
@synthesize headline;
@synthesize subhead;
@synthesize comments;
@synthesize remember;
@synthesize status;
@synthesize progress;
@synthesize cancel;
@synthesize send;

+ (NSString*) exceptionReport:(NSException*) exception
{
    NSMutableString* report = [NSMutableString new];
    NSMutableArray *addresses = [NSMutableArray new];
    NSString *stackTrace = [[exception userInfo] objectForKey:NSStackTraceKey];
    NSScanner *scanner = [NSScanner scannerWithString:stackTrace];
    NSString *token;
    
    [report appendString:[NSString stringWithFormat:@"%@ %@\n\n%@\n\n", exception.name, exception.reason, exception.userInfo]];
    
    while ([scanner scanUpToCharactersFromSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]
                                   intoString:&token])
    {
        [addresses addObject:token];
    }
    
    NSUInteger numFrames = [addresses count];
    if (numFrames > 0)
    {
        void **frames = (void **)malloc(sizeof(void *) * numFrames);
        NSUInteger i, parsedFrames;
        
        for (i = 0, parsedFrames = 0; i < numFrames; i++)
        {
            NSString *address = [addresses objectAtIndex:i];
            NSScanner* addressScanner = [NSScanner scannerWithString:address];
            
            if (![addressScanner scanHexLongLong:(unsigned long long *)&frames[parsedFrames]])
            {
                NSLog(@"%@ failed to parse frame address '%@'", [self className], address);
                break;
            }
            
            parsedFrames++;
        }
        
        if (parsedFrames > 0) {
            char **frameStrings = backtrace_symbols(frames, (int)parsedFrames);
            if (frameStrings)
            {
                for (unsigned i = 0; i < numFrames && frameStrings[i] ; i++)
                {
                    [report appendString:[NSString stringWithUTF8String:(char *)frameStrings[i]]];
                    [report appendString:@"\n"];
                }
                free(frameStrings);
            }
        }
        
        free(frames);
    }
    return report;
}

+ (NSString*) errorReport:(NSError*) error
{
    NSMutableString* report = [NSMutableString new];
    [report appendString:[NSString stringWithFormat:@"%@: %li\n\n%@", error.domain, (long)error.code, error.userInfo]];
    if( (error = [[error userInfo] objectForKey:NSUnderlyingErrorKey]) ) // we have to go deeper
    {
        [report appendString:[NSString stringWithFormat:@"\n\n- Underlying Error -\n\n"]];
        [report appendString:[self errorReport:error]];
    }
    return report;
}

+ (instancetype) windowForReporter:(PLCrashReporter*) reporter
{
    PLCrashReportWindow* window = [[PLCrashReportWindow alloc] initWithWindowNibName:[self className]];
    window.reporter = reporter;
    return window;
}

+ (instancetype) windowForReporter:(PLCrashReporter*) reporter withError:(NSError*) error
{
    PLCrashReportWindow* window = [[PLCrashReportWindow alloc] initWithWindowNibName:[self className]];
    window.reporter = reporter;
    window.error = error;
    return window;
}

+ (instancetype) windowForReporter:(PLCrashReporter*) reporter withException:(NSException*) exception
{
    PLCrashReportWindow* window = [[PLCrashReportWindow alloc] initWithWindowNibName:[self className]];
    window.reporter = reporter;
    window.exception = exception;
    return window;
}

- (void) runModal
{
    [super showWindow:self];
    [self.window orderFrontRegardless];
    self.modalSession = [NSApp beginModalSessionForWindow:self.window];
}

- (NSString*) grepSyslog // grep the syslog for any messages pertaining to us and return the messages

{
    NSString* appName = [[[NSBundle mainBundle] infoDictionary] objectForKey:(NSString*)kCFBundleNameKey];
    NSTask* grep = 	[NSTask new];
    NSPipe* output = [NSPipe pipe];
    [grep setLaunchPath:@"/usr/bin/grep"];
    [grep setArguments:@[appName, @"/var/log/system.log"]];
    [grep setStandardInput:[NSPipe pipe]];
    [grep setStandardOutput:output];
    [grep launch];
    NSString* logLines = [[NSString alloc] initWithData:[[output fileHandleForReading] readDataToEndOfFile] encoding:NSUTF8StringEncoding];
    return logLines;
}

- (void) restartApp
{
    static int fatal_signals[] = {SIGABRT, SIGBUS, SIGFPE, SIGILL, SIGSEGV, SIGTRAP };
    static int fatal_signals_count = (sizeof(fatal_signals) / sizeof(fatal_signals[0]));
    int pid = [[NSProcessInfo processInfo] processIdentifier];

    // clear out all the fatal signal handlers, so we don't end up crashing all the way down
    for (int i = 0; i < fatal_signals_count; i++) {
        struct sigaction sa;

        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = SIG_DFL;
        sigemptyset(&sa.sa_mask);

        sigaction(fatal_signals[i], &sa, NULL);
    }
    
    NSString* shellEscapedAppPath = [NSString stringWithFormat:@"'%@'", [[[NSBundle mainBundle] bundlePath] stringByReplacingOccurrencesOfString:@"'" withString:@"'\\''"]];
    NSString *script = [NSString stringWithFormat:@"(while /bin/kill -0 %d >&/dev/null; do /bin/sleep 0.1; done; /usr/bin/open %@) &", pid, shellEscapedAppPath];
    [[NSTask launchedTaskWithLaunchPath:@"/bin/sh" arguments:@[@"-c", script]] waitUntilExit];
}

- (void) postReportToWebServer:(NSURL*) approvedURL
{
    // the user has apporved the url if it's not secure, go ahead and upload via http or https
    NSMutableData* requestBody = [NSMutableData new];
    NSMutableURLRequest* uploadRequest = [NSMutableURLRequest requestWithURL:approvedURL];
    NSError* postError = nil;
    NSData* reportData = (self.reporter.hasPendingCrashReport
                         ?[self.reporter loadPendingCrashReportDataAndReturnError:&postError]
                         :[self.reporter generateLiveReportAndReturnError:&postError]);
    if( reportData)
    {
        PLCrashReport* report = [[PLCrashReport alloc] initWithData:reportData error:&postError];
        NSString* crashUUID = CFBridgingRelease(CFUUIDCreateString(kCFAllocatorDefault,report.uuidRef));
        NSString* crashFile = [crashUUID stringByAppendingPathExtension:@"plcrashreport"];
    
        NSString *boundary = [NSString stringWithFormat:@"---------------------------%@", crashUUID];;
        NSString *contentType = [NSString stringWithFormat:@"multipart/form-data; boundary=%@", boundary];

        [requestBody appendData:[[NSString stringWithFormat:@"--%@\r\n", boundary] dataUsingEncoding:NSUTF8StringEncoding]];
        [requestBody appendData:[[NSString stringWithFormat:@"Content-Disposition: form-data; comments=\"%@\" filename=\"%@\"\r\n",self.comments.textStorage.string,crashFile] dataUsingEncoding:NSUTF8StringEncoding]];
        [requestBody appendData:[@"Content-Type: application/octet-stream\r\n" dataUsingEncoding:NSUTF8StringEncoding]];
        [requestBody appendData:[@"Content-Transfer-Encoding: binary\r\n\r\n" dataUsingEncoding:NSUTF8StringEncoding]];
        [requestBody appendData:reportData];

        [uploadRequest addValue:contentType forHTTPHeaderField:@"Content-Type"];
        uploadRequest.cachePolicy = NSURLRequestReloadIgnoringCacheData;
        uploadRequest.HTTPShouldHandleCookies = NO;
        uploadRequest.timeoutInterval = 30;
        uploadRequest.HTTPMethod = @"POST";
        uploadRequest.HTTPBody = requestBody; // post data

        // add the comments, link the file and
        NSURLConnection* upload = [NSURLConnection connectionWithRequest:uploadRequest delegate:self];
        self.responseBody = [NSMutableData new];
        [upload scheduleInRunLoop:[NSRunLoop mainRunLoop] forMode:NSModalPanelRunLoopMode];
        [upload start];
    }
    else
    {
        NSLog(@"%@ no crash data error: %@ comments: %@", [self className], error, self.comments.textStorage.string);
    }
}

- (void) emailReportTo:(NSURL*) mailtoURL
{
    /* set ourself as the delegate to receive any errors */
    // mail.delegate = self;
    NSError* emailError = nil;
    NSData* reportData = (self.reporter.hasPendingCrashReport
                         ?[self.reporter loadPendingCrashReportDataAndReturnError:&emailError]
                         :[self.reporter generateLiveReportAndReturnError:&emailError]);
    PLCrashReport* report = [[PLCrashReport alloc] initWithData:reportData error:&emailError];
    NSString* crashUUID = CFBridgingRelease(CFUUIDCreateString(kCFAllocatorDefault,report.uuidRef));
    NSString* attachmentFilePath = [NSTemporaryDirectory() stringByAppendingPathComponent:[crashUUID stringByAppendingPathExtension:@"plcrashreport"]];
    [reportData writeToFile:attachmentFilePath atomically:NO];
    
    // requires scripting bridge header for Mail.app
    // see https://developer.apple.com/library/mac/samplecode/SBSendEmail/Introduction/Intro.html
    
    /* create a Scripting Bridge object for talking to the Mail application */
    MailApplication *mail = [SBApplication applicationWithBundleIdentifier:@"com.apple.Mail"];
    
    /* create a new outgoing message object */
    MailOutgoingMessage *emailMessage = [[[mail classForScriptingClass:@"outgoing message"] alloc] initWithProperties:
                                         [NSDictionary dictionaryWithObjectsAndKeys:
                                          [NSString stringWithFormat:@"Crash Report: %@", crashUUID], @"subject",
                                          self.comments.textStorage.string, @"content",
                                          nil]];
    
    /* add the object to the mail app  */
    [[mail outgoingMessages] addObject: emailMessage];
    
    /* set the sender, show the message */
    emailMessage.visible = YES;
    
    /* Test for errors */
    if ( [mail lastError] != nil )
        return;
    
    /* create a new recipient and add it to the recipients list */
    MailToRecipient *theRecipient = [[[mail classForScriptingClass:@"to recipient"] alloc] initWithProperties:
                                     [NSDictionary dictionaryWithObjectsAndKeys:
                                      mailtoURL.resourceSpecifier, @"address",
                                      nil]];
    [emailMessage.toRecipients addObject: theRecipient];
    
    /* Test for errors */
    if ( [mail lastError] != nil )
        return;
    
    /* add an attachment, if one was specified */
    
    if ( [attachmentFilePath length] > 0 ) {
        MailAttachment *theAttachment;
        
        theAttachment = [[[mail classForScriptingClass:@"attachment"] alloc] initWithProperties:
                         [NSDictionary dictionaryWithObjectsAndKeys:
                          [NSURL URLWithString:attachmentFilePath], @"fileName",
                          nil]];
        
        /* add it to the list of attachments */
        [[emailMessage.content attachments] addObject: theAttachment];
        
        /* Test for errors */
        if ( [mail lastError] != nil )
            return;
    }
    /* send the message */
    [emailMessage send];
    [self close];
}

- (void) sendReport
{
    // get the submission url
    NSURL* url = [NSURL URLWithString:[[[NSBundle bundleForClass:[self class]] infoDictionary] objectForKey:PLCrashWindowSubmitURLKey]];
    
    // if it's a mailto: create an email message with the support address
    if( [[url scheme] isEqualToString:@"mailto"])
    {
        [self emailReportTo:url];
    }
    else if( [[url scheme] isEqualToString:@"https"]) // if it's HTTPS post the crash report
    {
        [self postReportToWebServer:url];
    }
    else if( [[url scheme] isEqualToString:@"http"]) // // it it's HTTP prompt the user for permission to send
    {
        NSString* appName = [[[NSBundle mainBundle] infoDictionary] objectForKey:(NSString*)kCFBundleNameKey];
        NSURL* emailURL = [NSURL URLWithString:[[[NSBundle mainBundle] infoDictionary] objectForKey:PLCrashWindowSubmitEmailKey]];
        NSAlert* plaintextAlert = [NSAlert new];
        plaintextAlert.alertStyle = NSCriticalAlertStyle;
        plaintextAlert.messageText = PLLocalizedString(PLCrashWindowInsecureConnectionString);
        plaintextAlert.informativeText = [NSString stringWithFormat:PLLocalizedString(PLCrashWIndowInsecureConnectionInformationString), appName];
        [plaintextAlert addButtonWithTitle:PLLocalizedString(PLCrashWindowCancelString)];
        [plaintextAlert addButtonWithTitle:PLLocalizedString(PLCrashWindowSendString)];
        if( emailURL) // backup email key is specified
        {
            [plaintextAlert addButtonWithTitle:PLLocalizedString(PLCrashWindowEmailString)];
            plaintextAlert.informativeText = [plaintextAlert.informativeText stringByAppendingString:PLLocalizedString(PLCrashWindowInsecureConnectionEmailAlternateString)];
        }
        [plaintextAlert beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse returnCode)
        {
            if( returnCode == NSAlertFirstButtonReturn)
            {
                [self close];
            }
            else if( returnCode == NSAlertSecondButtonReturn)
            {
                [self postReportToWebServer:url];
            }
            else if( returnCode == NSAlertThirdButtonReturn) // backup email key is specified
            {
                [self emailReportTo:emailURL];
            }
        }];
    }
    else
    {
        NSBeep();
        NSLog(@"%@ unkown scheme: %@ comments: %@", [self className], url, self.comments.textStorage.string);
        [self close];
    }
}

- (void) reportConnectionError
{
    NSURL* emailURL = [NSURL URLWithString:[[[NSBundle mainBundle] infoDictionary] objectForKey:PLCrashWindowSubmitEmailKey]];
    if( emailURL) // backup email key is specified
    {
        NSString* appName = [[[NSBundle mainBundle] infoDictionary] objectForKey:(NSString*)kCFBundleNameKey];
        NSAlert* alert = [NSAlert new];
        alert.alertStyle = NSCriticalAlertStyle;
        alert.messageText = PLLocalizedString(PLCrashWindowSubmitFailedString);
        alert.informativeText = [NSString stringWithFormat:PLLocalizedString(PLCrashWindowSubmitFailedInformationString), appName, emailURL];
        [alert addButtonWithTitle:PLLocalizedString(PLCrashWindowEmailString)];
        [alert addButtonWithTitle:PLLocalizedString(PLCrashWindowCancelString)];
        [alert beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse returnCode)
         {
             if( returnCode == NSAlertFirstButtonReturn)
             {
                 [self emailReportTo:emailURL];
             }
             else if( returnCode == NSAlertSecondButtonReturn)
             {
                 [self close];
             }
         }];
    }
}

#pragma mark - NSWindowController

- (void) awakeFromNib
{
    // set the window title
    if( self.reporter.hasPendingCrashReport) self.window.title = PLLocalizedString(PLCrashWindowCrashReportString);
    else if( self.exception)                 self.window.title = PLLocalizedString(PLCrashWindowExceptionReportString);
    else if( self.error)                     self.window.title = PLLocalizedString(PLCrashWindowErrorReportString);

    // build the headline from the app name and event message
    NSString* appName = [[[NSBundle mainBundle] infoDictionary] objectForKey:(NSString*)kCFBundleNameKey];
    NSString* message = nil;
    
    if( self.reporter.hasPendingCrashReport) message = PLLocalizedString(PLCrashWindowCrashedString);
    else if( self.exception)                 message = PLLocalizedString(PLCrashWindowRaisedExceptionString);
    else if( self.error)                     message = PLLocalizedString(PLCrashWindowReportedErrorString);
    self.headline.stringValue = [NSString stringWithFormat:@"%@ %@", appName, message];

    // build the subhead from the app name, event message and dispostion strings
    if( self.reporter.hasPendingCrashReport) self.subhead.stringValue = PLLocalizedString(PLCrashWindowCrashDispositionString);
    else if( self.exception || self.error)   self.subhead.stringValue = PLLocalizedString(PLCrashWindowErrorDispositionString);
    
    if( self.reporter.hasPendingCrashReport) self.send.title = PLLocalizedString(PLCrashWindowReportString);
    else if( self.exception || self.error)
    {
        self.send.title = PLLocalizedString(PLCrashWindowRestartString);
        self.cancel.title = PLLocalizedString(PLCrashWindowQuitString);
    }
    
    [self.progress startAnimation:self];
    self.status.stringValue = @"";
    self.comments.editable = NO;
    self.remember.enabled = NO;
    self.send.enabled = NO;

    // fill in the comments section
    NSDictionary* commentsAttributes = @{NSFontAttributeName: [NSFont fontWithName:@"Menlo" size:9]};
    [self.comments.textStorage appendAttributedString:[[NSAttributedString alloc] initWithString:PLLocalizedString(PLCrashWindowCommentsString) attributes:commentsAttributes]];

    // if the error wasn't explicity set, grab the last one
    if( self.error )
    {
        [self.comments.textStorage appendAttributedString:[[NSAttributedString alloc] initWithString:@"\n\n- Error -\n\n" attributes:commentsAttributes]];
        [self.comments.textStorage appendAttributedString:[[NSAttributedString alloc] initWithString:[PLCrashReportWindow errorReport:self.error] attributes:commentsAttributes]];
    }
    
    if( self.exception)
    {
        [self.comments.textStorage appendAttributedString:[[NSAttributedString alloc] initWithString:@"\n\n- Exception -\n\n" attributes:commentsAttributes]];
        [self.comments.textStorage appendAttributedString:[[NSAttributedString alloc] initWithString:[PLCrashReportWindow exceptionReport:self.exception] attributes:commentsAttributes]];
    }
    
    // if the keys are set in the main bundle info keys, include the syslog and user defaults
    if( [[[[NSBundle mainBundle] infoDictionary] objectForKey:PLCrashWindowIncludeSyslogKey] boolValue])
    {
        [self.comments.textStorage appendAttributedString:[[NSAttributedString alloc] initWithString:@"\n\n- System Log -\n\n" attributes:commentsAttributes]];
        [self.comments.textStorage appendAttributedString:[[NSAttributedString alloc] initWithString:[self grepSyslog] attributes:commentsAttributes]];
    }
       
    if( [[[[NSBundle mainBundle] infoDictionary] objectForKey:PLCrashWindowIncludeDefaultsKey] boolValue])
    {
        NSString* defaultsString = [[[NSUserDefaults standardUserDefaults] persistentDomainForName:[[NSBundle mainBundle] bundleIdentifier]] description];
        [self.comments.textStorage appendAttributedString:[[NSAttributedString alloc] initWithString:@"\n\n- Application Defaults -\n\n" attributes:commentsAttributes]];
        [self.comments.textStorage appendAttributedString:[[NSAttributedString alloc] initWithString:defaultsString attributes:commentsAttributes]];
    }
    
    // select the 'please enter any notes' line for replacment
    [self.comments setSelectedRange:NSMakeRange(0,[self.comments.textStorage.string rangeOfCharacterFromSet:[NSCharacterSet newlineCharacterSet]].location)];
    
    self.comments.editable = YES;
    self.remember.enabled = YES;
    self.send.enabled = YES;
    [self.progress stopAnimation:self];
    
    if( [[NSUserDefaults standardUserDefaults] boolForKey:PLCrashWindowAutoSubmitKey]) // auto submit now that the report is generated
    {
        self.remember.state = NSOnState;
        [self performSelector:@selector(onSend:) withObject:self afterDelay:0]; // present the window and send the report, showing them that we're doing it, they can canel and add comments
    }
}

#pragma mark - NSWindowDelegate

- (void)windowWillClose:(NSNotification *)notification
{
    if( self.modalSession)
        [NSApp endModalSession:self.modalSession];
    
    if( self.error || self.exception)
    {
#ifdef DEBUG
        [self restartApp];
#else
        exit(-1);
#endif
    }
}

#pragma mark - IBActions

- (IBAction)onCancel:(id)sender
{
    // are we currently sending? stop that
    self.comments.editable = YES;
    self.remember.enabled = YES;
    self.send.enabled = YES;
    self.status.stringValue = @"";
    [self.progress stopAnimation:self];

    [self close];
}

- (IBAction)onSend:(id)sender
{
    // check remember button & save preferences
    if( self.remember.state ) [[NSUserDefaults standardUserDefaults] setBool:YES forKey:PLCrashWindowAutoSubmitKey];
    
    // start the progress indicator and disable various controls
    [self.progress startAnimation:self];
    self.comments.editable = NO;
    self.remember.enabled = NO;
    self.send.enabled = NO;

    // perform the upload
    [self sendReport];
}

#pragma mark - NSURLConnectionDataDelegate

- (void)connection:(NSURLConnection *)connection didReceiveResponse:(NSURLResponse *)URLresponse
{
    if( [URLresponse isKindOfClass:[NSHTTPURLResponse class]])
    {
        self.response = (NSHTTPURLResponse*)URLresponse;
    }
}

- (void)connection:(NSURLConnection *)connection
   didSendBodyData:(NSInteger)bytesWritten
 totalBytesWritten:(NSInteger)totalBytesWritten
totalBytesExpectedToWrite:(NSInteger)totalBytesExpectedToWrite
{
    // TODO progress indicator
    self.status.stringValue = [NSString stringWithFormat:@"%li/%li %C",(long)totalBytesWritten,(long)totalBytesExpectedToWrite, 0x2191]; // UPWARDS ARROW Unicode: U+2191, UTF-8: E2 86 91
    NSLog(@"%@ post: %li/%li bytes", [self className], (long)totalBytesWritten,(long)totalBytesExpectedToWrite);
}

- (void)connection:(NSURLConnection *)connection didReceiveData:(NSData *)data
{
    [self.responseBody appendData:data];
    self.status.stringValue = [NSString stringWithFormat:@"%li %C",(unsigned long)self.responseBody.length, 0x2193]; //↓ DOWNWARDS ARROW Unicode: U+2193, UTF-8: E2 86 93
    NSLog(@"%@ read: %li bytes", self.className, (unsigned long)self.responseBody.length);
}

- (void)connectionDidFinishLoading:(NSURLConnection *) connection
{
    NSString* bodyString = [[NSString alloc] initWithData:self.responseBody encoding:NSUTF8StringEncoding];
    NSLog(@"%@ response: %li body: %@", [self className], (long)self.response.statusCode, bodyString);

    if( self.response.statusCode == 200 ) // OK!
    {
        self.status.stringValue = [NSString stringWithFormat:@"%C", 0x2713]; // CHECK MARK Unicode: U+2713, UTF-8: E2 9C 93
        [self.reporter purgePendingCrashReport];
        [self close];
    }
    else // not ok, present error
    {
        self.status.stringValue = [NSString stringWithFormat:@"%li %C", (long)self.response.statusCode, 0x274C];// CROSS MARK Unicode: U+274C, UTF-8: E2 9D 8C
        [self reportConnectionError]; // offer to email or cancel
    }

}

#pragma mark - NSURlConnectionDelegate

- (void)connection:(NSURLConnection *)connection didFailWithError:(NSError *)connectionError
{
    NSBeep();
    self.status.stringValue = [NSString stringWithFormat:@"%C", 0x29F1];// ERROR-BARRED BLACK DIAMOND Unicode: U+29F1, UTF-8: E2 A7 B1
    
    if( connectionError ) // log it to the comments
    {
        NSDictionary* commentsAttributes = @{NSFontAttributeName: [NSFont fontWithName:@"Menlo" size:9]};
        [self.comments.textStorage appendAttributedString:[[NSAttributedString alloc] initWithString:@"\n\n- Error -\n\n" attributes:commentsAttributes]];
        [self.comments.textStorage appendAttributedString:[[NSAttributedString alloc] initWithString:[PLCrashReportWindow errorReport:connectionError] attributes:commentsAttributes]];
    }

    [self reportConnectionError]; // offer to email or cancel
}

- (BOOL)connectionShouldUseCredentialStorage:(NSURLConnection *)connection
{
    return NO;
}

@end
