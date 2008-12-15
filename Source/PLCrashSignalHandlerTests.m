/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008 Plausible Labs Cooperative, Inc.
 * All rights reserved.
 */

#import "GTMSenTestCase.h"

#import "PLCrashSignalHandler.h"

@interface PLCrashSignalHandlerTests : SenTestCase @end

static void crash_callback (int signal, siginfo_t *siginfo, ucontext_t *uap, void *context) {
    // Do nothing
}

@implementation PLCrashSignalHandlerTests

- (void) testSharedHandler {
    STAssertNotNil([PLCrashSignalHandler sharedHandler], @"Nil shared handler");
}

- (void) testRegisterSignalHandlers {
    NSError *error;
    struct sigaction action;
    
    /* Ensure that no signal handler is registered for one of the fatal signals */
    sigaction (SIGSEGV, NULL, &action);
    STAssertEquals(action.sa_handler, SIG_DFL, @"Action already registered for SIGSEGV");
    
    /* Register the signal handlers */
    STAssertTrue([[PLCrashSignalHandler sharedHandler] registerHandlerWithCallback: &crash_callback context: NULL error: &error], @"Could not register signal handler: %@", error);
    
    /* Check for SIGSEGV registration */
    sigaction (SIGSEGV, NULL, &action);
    STAssertNotEquals(action.sa_handler, SIG_DFL, @"Action not registered for SIGSEGV");
}

@end
