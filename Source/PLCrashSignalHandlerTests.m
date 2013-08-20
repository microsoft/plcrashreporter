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
#import "PLCrashProcessInfo.h"

@interface PLCrashSignalHandlerTests : SenTestCase {
}
@end

/* Page-aligned to allow us to tweak memory protections for test purposes. */
static uint8_t crash_page[PAGE_SIZE] __attribute__((aligned(PAGE_SIZE)));

static bool crash_callback (int signal, siginfo_t *siginfo, ucontext_t *uap, plcrash_signal_handler_callback_set_t *next, void *context) {
    mprotect(crash_page, sizeof(crash_page), PROT_READ|PROT_WRITE);
    
    // Success
    crash_page[1] = 0xFE;
    
    return true;
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

    /* Verify that the callback is dispatched; if the process doesn't lock up here, the signal handler is working. Unfortunately, this
     * test will halt when run under a debugger due to their catching of fatal signals, so we only perform the test if we're not
     * currently being traced */
    if (![[PLCrashProcessInfo currentProcessInfo] isTraced]) {
        mprotect(crash_page, sizeof(crash_page), 0);
        crash_page[0] = 0xCA;
        
        STAssertEquals(crash_page[0], (uint8_t)0xCA, @"Byte should have been set to test value");
        STAssertEquals(crash_page[1], (uint8_t)0xFE, @"Crash callback did not run");
    } else {
        NSLog(@"Running under debugger; skipping signal callback validation.");
    }
}

@end
