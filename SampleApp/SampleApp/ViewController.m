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

@interface ViewController ()

@end

void callbackSigHandler(siginfo_t *info, ucontext_t *uap, void *context) {
    os_log_debug(OS_LOG_DEFAULT, "[DEBUG][callbackSigHandler] callbackSigHandler start");
    
    dispatch_semaphore_t send_crash_callback_lock = dispatch_semaphore_create(0);
    
    dispatch_sync(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0), ^{
        os_log_debug(OS_LOG_DEFAULT, "[DEBUG][dispatch_async] dispatch_get_main_queue Start");
        
        
        os_log_debug(OS_LOG_DEFAULT, "[DEBUG][dispatch_async] dispatch_get_main_queue End");
        dispatch_semaphore_signal(send_crash_callback_lock);
    });
    
    dispatch_time_t callback_timeout = dispatch_time(DISPATCH_TIME_NOW, 3 * 1000000000ull);
    
    dispatch_semaphore_wait(send_crash_callback_lock, callback_timeout);
}


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
    
    int n = 10 / 0;
}

- (void)didReceiveMemoryWarning {
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}


@end
