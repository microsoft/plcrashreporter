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

#import "PLCrashMachExceptionServer.h"

#include <sys/mman.h>

@interface PLCrashMachExceptionServerTests : SenTestCase @end

@implementation PLCrashMachExceptionServerTests

static uint8_t crash_page[PAGE_SIZE] __attribute__((aligned(PAGE_SIZE)));

static bool exception_callback (task_t task,
                                thread_t thread,
                                exception_type_t exception_type,
                                mach_exception_data_t code,
                                mach_msg_type_number_t code_count,
                                bool double_fault,
                                void *context)
{
    mprotect(crash_page, sizeof(crash_page), PROT_READ|PROT_WRITE);
    
    if (code_count != 2) {
        crash_page[1] = 0xFA;
    } else if (code[1] != (uintptr_t) crash_page) {
        crash_page[1] = 0xFB;
    } else {
        // Success
        crash_page[1] = 0xFE;
    }

    BOOL *didRun = (BOOL *) context;
    if (didRun != NULL)
        *didRun = YES;

    return true;
}

/**
 * Test inserting/removing the task mach exception server from the handler chain.
 */
- (void) testTaskServerInsertion {
    NSError *error;

    PLCrashMachExceptionServer *server = [[[PLCrashMachExceptionServer alloc] init] autorelease];
    STAssertNotNil(server, @"Failed to initialize server");

    STAssertTrue([server registerHandlerForTask: mach_task_self()
                                         thread: MACH_PORT_NULL
                                   withCallback: exception_callback
                                        context: NULL
                                          error: &error], @"Failed to configure handler: %@", error);

    mprotect(crash_page, sizeof(crash_page), 0);

    /* If the test doesn't lock up here, it's working */
    crash_page[0] = 0xCA;

    STAssertEquals(crash_page[0], (uint8_t)0xCA, @"Page should have been set to test value");
    STAssertEquals(crash_page[1], (uint8_t)0xFE, @"Crash callback did not run");

    STAssertTrue([server deregisterHandlerAndReturnError: &error], @"Failed to reset handler; %@", error);
}

/**
 * Test inserting/removing the mach exception server from the handler chain.
 */
- (void) testThreadServerInsertion {
    NSError *error;
    
    PLCrashMachExceptionServer *task = [[[PLCrashMachExceptionServer alloc] init] autorelease];
    STAssertNotNil(task, @"Failed to initialize server");

    PLCrashMachExceptionServer *thr = [[[PLCrashMachExceptionServer alloc] init] autorelease];
    STAssertNotNil(thr, @"Failed to initialize server");

    BOOL taskRan = false;
    BOOL threadRan = false;

    STAssertTrue([task registerHandlerForTask: mach_task_self()
                                       thread: MACH_PORT_NULL
                                 withCallback: exception_callback
                                      context: &taskRan
                                        error: &error], @"Failed to configure handler: %@", error);

    STAssertTrue([thr registerHandlerForTask: mach_task_self()
                                      thread: mach_thread_self()
                                withCallback: exception_callback
                                     context: &threadRan
                                       error: &error], @"Failed to configure handler: %@", error);

    mprotect(crash_page, sizeof(crash_page), 0);
    
    /* If the test doesn't lock up here, it's working */
    crash_page[0] = 0xCA;
    
    STAssertEquals(crash_page[0], (uint8_t)0xCA, @"Page should have been set to test value");
    STAssertEquals(crash_page[1], (uint8_t)0xFE, @"Crash callback did not run");

    STAssertFalse(taskRan, @"Task handler ran");
    STAssertTrue(threadRan, @"Thread-specific handler did not run");
    
    STAssertTrue([thr deregisterHandlerAndReturnError: &error], @"Failed to reset handler; %@", error);
    STAssertTrue([task deregisterHandlerAndReturnError: &error], @"Failed to reset handler; %@", error);
}

@end
