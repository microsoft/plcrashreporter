/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008 Plausible Labs Cooperative, Inc.
 * All rights reserved.
 */

#import <Foundation/Foundation.h>

#import <mach/mach.h>
#import <mach/exception.h>

/**
 * @internal
 * Saved reference to the previous exception handler.
 */
typedef struct PLCrashSignalHandlerPorts {
    mach_msg_type_number_t  count;
    exception_mask_t        masks[EXC_TYPES_COUNT];
    exception_port_t        ports[EXC_TYPES_COUNT];
    exception_behavior_t    behaviors[EXC_TYPES_COUNT];
    thread_state_flavor_t   flavors[EXC_TYPES_COUNT];
} PLCrashSignalHandlerPorts;

@interface PLCrashSignalHandler : NSObject {
@private
    /** Exception port */
    mach_port_t _exceptionPort;

    /** Handler to which exceptions should be forwarded */
    PLCrashSignalHandlerPorts _forwardingHandler;
}

+ (PLCrashSignalHandler *) sharedHandler;
- (BOOL) registerHandlerAndReturnError: (NSError **) outError;

- (void) testHandlerWithSignal: (int) signal code: (int) code faultAddress: (void *) address;

@end
