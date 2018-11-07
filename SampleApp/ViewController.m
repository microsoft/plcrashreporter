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

void callbackSigHandler(siginfo_t *info, ucontext_t *uap, void *context);

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
            if (crashLog == nil)
            {
                [crashReporter purgePendingCrashReport];
                return;
            }
        }
        
        os_log_info(OS_LOG_DEFAULT, "[DEBUG][dispatch_async] dispatch_get_main_queue End");
        dispatch_semaphore_signal(send_crash_callback_lock);
    });
    
    dispatch_time_t callback_timeout = dispatch_time(DISPATCH_TIME_NOW, 3 * 1000000000ull);
    
    dispatch_semaphore_wait(send_crash_callback_lock, callback_timeout);
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
