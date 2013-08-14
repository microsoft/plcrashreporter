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

#if PLCRASH_FEATURE_MACH_EXCEPTIONS

#import "GTMSenTestCase.h"
#import "PLCrashMachExceptionPortState.h"

@interface PLCrashMachExceptionPortStateTests : SenTestCase {

}
@end

@implementation PLCrashMachExceptionPortStateTests

- (void) testExceptionPortStatesForTask {
    plcrash_mach_exception_port_state_set_t states;
    NSError *error;
    kern_return_t kr;
    
    /* Fetch the current ports */
    kr = task_get_exception_ports(mach_task_self(), EXC_MASK_ALL, states.masks, &states.count, states.ports, states.behaviors, states.flavors);
    
    PLCrashMachExceptionPortStateSet *objStates = [PLCrashMachExceptionPortState exceptionPortStatesForTask: mach_task_self() mask: EXC_MASK_ALL error: &error];
    STAssertNotNil(objStates, @"Failed to fetch port state: %@", error);

    /* Compare the sets */
    STAssertEquals([objStates.set count], (NSUInteger) states.count, @"Incorrect count");
    for (PLCrashMachExceptionPortState *state in objStates) {
        BOOL found = NO;
        for (mach_msg_type_number_t i = 0; i < states.count; i++) {
            if (states.masks[i] != state.mask)
                continue;
            
            found = YES;
            STAssertEquals(states.ports[i], state.port, @"Incorrect port");
            STAssertEquals(states.behaviors[i], state.behavior, @"Incorrect behavior");
            STAssertEquals(states.flavors[i], state.flavor, @"Incorrect flavor");
        }
        STAssertTrue(found, @"State not found");
    }
}

- (void) testExceptionPortStatesForThread {
    plcrash_mach_exception_port_state_set_t states;
    NSError *error;
    kern_return_t kr;
    
    /* Fetch the current ports */
    kr = thread_get_exception_ports(mach_thread_self(), EXC_MASK_ALL, states.masks, &states.count, states.ports, states.behaviors, states.flavors);
    
    PLCrashMachExceptionPortStateSet *objStates = [PLCrashMachExceptionPortState exceptionPortStatesForThread: mach_thread_self() mask: EXC_MASK_ALL error: &error];
    STAssertNotNil(objStates, @"Failed to fetch port state: %@", error);
    
    /* Compare the sets */
    STAssertEquals([objStates.set count], (NSUInteger) states.count, @"Incorrect count");
    for (PLCrashMachExceptionPortState *state in objStates) {
        BOOL found = NO;
        for (mach_msg_type_number_t i = 0; i < states.count; i++) {
            if (states.masks[i] != state.mask)
                continue;
            
            found = YES;
            STAssertEquals(states.ports[i], state.port, @"Incorrect port");
            STAssertEquals(states.behaviors[i], state.behavior, @"Incorrect behavior");
            STAssertEquals(states.flavors[i], state.flavor, @"Incorrect flavor");
        }
        STAssertTrue(found, @"State not found");
    }
}

- (void) testRegisterForTask {
    NSError *error;
    PLCrashMachExceptionPortStateSet *previousStates;

    PLCrashMachExceptionPortState *state = [[[PLCrashMachExceptionPortState alloc] initWithPort: MACH_PORT_NULL
                                                                                           mask: EXC_MASK_SOFTWARE
                                                                                       behavior: EXCEPTION_STATE_IDENTITY
                                                                                         flavor: MACHINE_THREAD_STATE] autorelease];

    /* Fetch the current state to compare against */
    PLCrashMachExceptionPortStateSet *initialState = [PLCrashMachExceptionPortState exceptionPortStatesForTask: mach_task_self() mask: EXC_MASK_SOFTWARE error: &error];
    STAssertNotNil(initialState, @"Failed to fetch port state: %@", error);
    
    /* Set new state */
    STAssertTrue([state registerForTask: mach_task_self() previousPortStates: &previousStates error: &error], @"Failed to register exception ports: %@", error);
    
    /* Verify that new state matches our expectations */
    PLCrashMachExceptionPortStateSet *newState = [PLCrashMachExceptionPortState exceptionPortStatesForTask: mach_task_self() mask: EXC_MASK_SOFTWARE error: &error];
    for (PLCrashMachExceptionPortState *expected in newState) {
        STAssertEquals((mach_port_t)MACH_PORT_NULL, expected.port, @"Incorrect port");
    }

    /* Restore */
    for (PLCrashMachExceptionPortState *prev in previousStates)
        STAssertTrue([prev registerForTask: mach_task_self() previousPortStates: NULL error: &error], @"Failed to restore port: %@", error);

    /* Verify that final state matches our expectations */
    for (PLCrashMachExceptionPortState *expected in initialState) {
        for (PLCrashMachExceptionPortState *prev in previousStates)
            if (prev.mask == expected.mask)
                STAssertEquals(expected.port, prev.port, @"Incorrect port restored");
    }
}

- (void) testRegisterForThread {
    NSError *error;
    PLCrashMachExceptionPortStateSet *previousStates;
    
    PLCrashMachExceptionPortState *state = [[[PLCrashMachExceptionPortState alloc] initWithPort: MACH_PORT_NULL
                                                                                           mask: EXC_MASK_SOFTWARE
                                                                                       behavior: EXCEPTION_STATE_IDENTITY
                                                                                         flavor: MACHINE_THREAD_STATE] autorelease];
    
    /* Fetch the current state to compare against */
    PLCrashMachExceptionPortStateSet *initialState = [PLCrashMachExceptionPortState exceptionPortStatesForThread: mach_thread_self() mask: EXC_MASK_SOFTWARE error: &error];
    STAssertNotNil(initialState, @"Failed to fetch port state: %@", error);
    
    /* Set new state */
    STAssertTrue([state registerForThread: mach_thread_self() previousPortStates: &previousStates error: &error], @"Failed to register exception ports: %@", error);
    
    /* Verify that new state matches our expectations */
    PLCrashMachExceptionPortStateSet *newState = [PLCrashMachExceptionPortState exceptionPortStatesForThread: mach_thread_self() mask: EXC_MASK_SOFTWARE error: &error];
    for (PLCrashMachExceptionPortState *expected in newState) {
        STAssertEquals((mach_port_t)MACH_PORT_NULL, expected.port, @"Incorrect port");
    }
    
    /* Restore */
    for (PLCrashMachExceptionPortState *prev in previousStates)
        STAssertTrue([prev registerForThread: mach_thread_self() previousPortStates: NULL error: &error], @"Failed to restore port: %@", error);
    
    /* Verify that final state matches our expectations */
    for (PLCrashMachExceptionPortState *expected in initialState) {
        for (PLCrashMachExceptionPortState *prev in previousStates)
            if (prev.mask == expected.mask)
                STAssertEquals(expected.port, prev.port, @"Incorrect port restored");
    }
}

@end

#endif /* PLCRASH_FEATURE_MACH_EXCEPTIONS */
