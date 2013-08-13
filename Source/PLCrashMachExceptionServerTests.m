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

#include "PLCrashReporterBuildConfig.h"

#if 0 && PLCRASH_FEATURE_MACH_EXCEPTIONS

#import "GTMSenTestCase.h"

#import "PLCrashMachExceptionServer.h"
#import "PLCrashHostInfo.h"

#include <sys/mman.h>

#ifndef EXC_MASK_RESOURCE
#define EXC_MASK_RESOURCE (1<<11)
#endif

@interface PLCrashMachExceptionServerTests : SenTestCase {
    plcrash_mach_exception_port_state_t _task_ports;
    plcrash_mach_exception_port_state_t _thread_ports;
}
@end

@implementation PLCrashMachExceptionServerTests

- (void) setUp {
    /* The iOS Simulator SDK includes EXC_MASK_GUARD in EXC_MASK_ALL, but the
     * host system (eg, Mac OS X <= 10.8) may not support it, in which case we
     * need to strip the flag out here.
     *
     * We also ignore EXC_RESOURCE entirely (it's not monitored by the crash
     * reporter).
     */
    exception_mask_t exc_mask_all = EXC_MASK_ALL;
    
#ifdef EXC_MASK_RESOURCE
    exc_mask_all &= ~EXC_MASK_RESOURCE;
#endif

#if defined(EXC_MASK_GUARD) && TARGET_IPHONE_SIMULATOR

# if MAC_OS_X_VERSION_MIN_REQUIRED > MAC_OS_X_VERSION_10_8
#   error This work-around is no longer required and should be removed
# endif

    if ([PLCrashHostInfo currentHostInfo].darwinVersion.major < PLCRASH_HOST_MAC_OS_X_DARWIN_MAJOR_VERSION_10_9)
        exc_mask_all &= ~EXC_MASK_GUARD;
#endif

    /*
     * Reset the current exception ports. Our tests interfere with any observing
     * debuggers, so we remove any task and thread exception ports here, and then
     * restore them in -tearDown.
     */
    kern_return_t kr;
    kr = task_swap_exception_ports(mach_task_self(),
                                   exc_mask_all,
                                   MACH_PORT_NULL,
                                   EXCEPTION_DEFAULT,
                                   THREAD_STATE_NONE,
                                   _task_ports.masks,
                                   &_task_ports.count,
                                   _task_ports.ports,
                                   _task_ports.behaviors,
                                   _task_ports.flavors);
    STAssertEquals(kr, KERN_SUCCESS, @"Failed to reset task ports");
    
    kr = thread_swap_exception_ports(mach_thread_self(),
                                     exc_mask_all,
                                     MACH_PORT_NULL,
                                     EXCEPTION_DEFAULT,
                                     THREAD_STATE_NONE,
                                     _thread_ports.masks,
                                     &_thread_ports.count,
                                     _thread_ports.ports,
                                     _thread_ports.behaviors,
                                     _thread_ports.flavors);
    STAssertEquals(kr, KERN_SUCCESS, @"Failed to reset thread ports");
}

- (void) tearDown {
    kern_return_t kr;

    /* Restore the original exception ports */
    for (mach_msg_type_number_t i = 0; i < _task_ports.count; i++) {
        if (MACH_PORT_VALID(!_task_ports.ports[i]))
            continue;
    
        kr = task_set_exception_ports(mach_task_self(), _task_ports.masks[i], _task_ports.ports[i], _task_ports.behaviors[i], _task_ports.flavors);
        STAssertEquals(kr, KERN_SUCCESS, @"Failed to set task ports");
    }
    
    for (mach_msg_type_number_t i = 0; i < _thread_ports.count; i++) {
        if (MACH_PORT_VALID(!_thread_ports.ports[i]))
            continue;
        
        kr = thread_set_exception_ports(mach_thread_self(), _thread_ports.masks[i], _thread_ports.ports[i], _thread_ports.behaviors[i], _thread_ports.flavors);
        STAssertEquals(kr, KERN_SUCCESS, @"Failed to set thread ports");
    }
}

static uint8_t crash_page[PAGE_SIZE] __attribute__((aligned(PAGE_SIZE)));

static bool exception_callback (task_t task,
                                thread_t thread,
                                exception_type_t exception_type,
                                mach_exception_data_t code,
                                mach_msg_type_number_t code_count,
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

/* An exception callback that simply exits with a return code of 25 */
static bool exception_callback_exit (task_t task,
                                     thread_t thread,
                                     exception_type_t exception_type,
                                     mach_exception_data_t code,
                                     mach_msg_type_number_t code_count,
                                     void *context)
{
    if (task != mach_task_self()) {
        /* Our callback was executed for a child process */
        exit(25);
    }

    return false;
}

/*
 * Verify that child process' exceptions are passed to the originally
 * registered exception handler.
 */
- (void) testChildInheritedExceptionHandler {
    NSError *error;

    /* Register an inheritable server */
    PLCrashMachExceptionServer *server = [[[PLCrashMachExceptionServer alloc] init] autorelease];
    STAssertNotNil(server, @"Failed to initialize server");
    
    STAssertTrue([server registerHandlerForTask: mach_task_self()
                                         thread: MACH_PORT_NULL
                                   withCallback: exception_callback_exit
                                        context: NULL
                                          error: &error], @"Failed to configure handler: %@", error);
    
    
    mprotect(crash_page, sizeof(crash_page), PROT_NONE);
    pid_t pid = fork();
    if (pid == -1) {
        /* Failure is expected on iOS */
#if !TARGET_OS_IPHONE
        const char *errstr = strerror(errno);
        STFail(@"Fork failed: %s", errstr);
#endif
    } else if (pid != 0) {
        /* In parent */
        int statl;
        
        /* Wait for termination */
        pid_t wpid;
        do {
            wpid = waitpid(pid, &statl, 0);
        } while (wpid == -1 && errno == EINTR);
        STAssertEquals(wpid, pid, @"waitpid() failed: %s", strerror(errno));
        
        /* Verify that termination occured due to an unhandled signal */
        STAssertTrue(WIFSIGNALED(statl), @"Child process should have failed with a signal");
        STAssertNotEquals(WEXITSTATUS(statl), 25, @"Child process triggered callback execution!");
        STAssertNotEquals(WEXITSTATUS(statl), 26, @"Child process exited cleanly!");

    } else {
        /* In child; trigger a crash */
        crash_page[0] = 0xCA;

        /* Should be unreachable */
        exit(26);
    }
    
    STAssertTrue([server deregisterHandlerAndReturnError: &error], @"Failed to reset handler; %@", error);
}

@end

#endif /* PLCRASH_FEATURE_MACH_EXCEPTIONS */
