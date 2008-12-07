/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008 Plausible Labs Cooperative, Inc.
 * All rights reserved.
 */

#import "GTMSenTestCase.h"

#import "PLCrashSignalHandler.h"

@interface PLCrashSignalHandlerTests : SenTestCase @end

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
    STAssertTrue([[PLCrashSignalHandler sharedHandler] registerHandlerAndReturnError: &error], @"Could not register signal handler: %@", error);

    /* Check for SIGSEGV registration */
    sigaction (SIGSEGV, NULL, &action);
    STAssertNotEquals(action.sa_handler, SIG_DFL, @"Action not registered for SIGSEGV");
}

- (void) testSignalHandler {
    /* Send a test 'signal' to the handler. Nothing for us to verify afterwards, yet. */
    [[PLCrashSignalHandler sharedHandler] testHandlerWithSignal: SIGBUS code: SEGV_MAPERR faultAddress: 0x0];
}

@end
