//
//  ViewController.m
//  SampleApp
//
//  Created by KangYongseok on 07/11/2018.
//  Copyright © 2018 Yongseok Kang. All rights reserved.
//

#import "ViewController.h"
@import CrashReporter;
@import os.log;
#import "Formatter.h"

void callbackSigHandler(siginfo_t *info, ucontext_t *uap, void *context);
void report(PLCrashReport *report);

void callbackSigHandler(siginfo_t *info, ucontext_t *uap, void *context) {
    os_log_info(OS_LOG_DEFAULT, "post crash callback: signo=%d, uap=%p, context=%p", info->si_signo, uap, context);
    os_log_info(OS_LOG_DEFAULT, "[DEBUG][callbackSigHandler] callbackSigHandler start");
    
    dispatch_semaphore_t send_crash_callback_lock = dispatch_semaphore_create(0);
    
    dispatch_sync(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0), ^{
        os_log_info(OS_LOG_DEFAULT, "[DEBUG][dispatch_async] dispatch_get_main_queue Start");
        
        PLCrashReporterSignalHandlerType signalHandlerType = PLCrashReporterSignalHandlerTypeBSD;
        PLCrashReporterSymbolicationStrategy symbolicationStrategy = PLCrashReporterSymbolicationStrategyAll;
        PLCrashReporterConfig *config = [[PLCrashReporterConfig alloc] initWithSignalHandlerType: signalHandlerType
                                                                           symbolicationStrategy: symbolicationStrategy];
        PLCrashReporter *crashReporter = [[PLCrashReporter alloc] initWithConfiguration: config];
        
        if ([crashReporter hasPendingCrashReport]) {
            os_log_info(OS_LOG_DEFAULT, "[INFO][sendCrashReport] crashReporter hasPendingCrashReport");
            
            NSError *error = NULL;
            NSData *crashData = [[NSData alloc] initWithData:[crashReporter loadPendingCrashReportDataAndReturnError: &error]];
            PLCrashReport *crashLog = [[PLCrashReport alloc] initWithData: crashData error: &error];
            if (crashLog == nil) {
                [crashReporter purgePendingCrashReport];
                return;
            }
            
            report(crashLog);
        }
        
        os_log_info(OS_LOG_DEFAULT, "[DEBUG][dispatch_async] dispatch_get_main_queue End");
        dispatch_semaphore_signal(send_crash_callback_lock);
    });
    
    dispatch_time_t callback_timeout = dispatch_time(DISPATCH_TIME_NOW, 3 * 1000000000ull);
    
    dispatch_semaphore_wait(send_crash_callback_lock, callback_timeout);
}

void report(PLCrashReport *report) {
    NSMutableString* text = [NSMutableString string];
    boolean_t lp64 = true;
    
    [text appendString:@"\n"];
    PLCrashReportThreadInfo *crashed_thread = nil;
    NSInteger maxThreadNum = 0;
    for (PLCrashReportThreadInfo *thread in report.threads) {
        if (thread.crashed) {
            [text appendFormat: @"Thread %ld Crashed:\n", (long) thread.threadNumber];
            crashed_thread = thread;
        } else {
            [text appendFormat: @"Thread %ld:\n", (long) thread.threadNumber];
        }
        for (NSUInteger frame_idx = 0; frame_idx < [thread.stackFrames count]; frame_idx++) {
            PLCrashReportStackFrameInfo *frameInfo = [thread.stackFrames objectAtIndex: frame_idx];
            [text appendString: [Formatter formatStackFrame: frameInfo frameIndex: frame_idx report: report lp64: lp64]];
        }
        [text appendString: @"\n"];
        
        /* Track the highest thread number */
        maxThreadNum = MAX(maxThreadNum, thread.threadNumber);
    }
    
    os_log_info(OS_LOG_DEFAULT, "%@", text);
}

@interface ViewController ()

@end

@implementation ViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    
    PLCrashReporterSignalHandlerType signalHandlerType = PLCrashReporterSignalHandlerTypeBSD;
    PLCrashReporterSymbolicationStrategy symbolicationStrategy = PLCrashReporterSymbolicationStrategyAll;
    PLCrashReporterConfig *config = [[PLCrashReporterConfig alloc] initWithSignalHandlerType: signalHandlerType
                                                                       symbolicationStrategy: symbolicationStrategy];
    PLCrashReporter *crashReporter = [[PLCrashReporter alloc] initWithConfiguration: config];
    
    @try {
        //define the callback function for PLCrashReporter
        PLCrashReporterCallbacks cb = {
            .version = 0,
            .context = NULL,
            .handleSignal = callbackSigHandler
        };
        
        NSError *error = NULL;
        //install the callback function
        [crashReporter setCrashCallbacks: &cb];
        if (![crashReporter enableCrashReporterAndReturnError: &error])
        {
            NSLog(@"[WARN][NELO2-init] Could not enable crash reporter: %@", error);
        }
    } @catch (NSException *e) {
        NSLog(@"[WARN][NELO2-init] Failed to enable crash report:%@", e);
    }
    os_log_debug(OS_LOG_DEFAULT, "test");
}

- (void)viewDidAppear:(BOOL)animated {
    [super viewDidAppear:animated];
    
    @throw @"exception test";
}

- (void)didReceiveMemoryWarning {
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}


@end
