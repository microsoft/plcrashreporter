/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008-2009 Plausible Labs Cooperative, Inc.
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
