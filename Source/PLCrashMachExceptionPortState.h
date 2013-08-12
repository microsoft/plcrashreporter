/*
 * Author: Landon Fuller <landonf@bikemonkey.org>
 *
 * Copyright (c) 2012-2013 Plausible Labs Cooperative, Inc.
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

#import <Foundation/Foundation.h>
#import <mach/mach.h>

#include "PLCrashReporterBuildConfig.h"

#if PLCRASH_FEATURE_MACH_EXCEPTIONS

/**
 * @internal
 *
 * A pure C representation of exception state as returned by task_get_exception_ports(). Up
 * to EXC_TYPES_COUNT entries may be returned. The actual count is provided via
 * plcrash_mach_exception_port_state::count. The values stored in the arrays correspond
 * positionally.
 */
typedef struct plcrash_mach_exception_port_state {
    /** Number of independent mask/port/behavior/flavor sets
     * (up to EXC_TYPES_COUNT). */
    mach_msg_type_number_t count;
    
    /** Exception masks. */
    exception_mask_t masks[EXC_TYPES_COUNT];
    
    /** Exception ports. */
    mach_port_t ports[EXC_TYPES_COUNT];
    
    /** Exception behaviors. */
    exception_behavior_t behaviors[EXC_TYPES_COUNT];
    
    /** Exception thread flavors. */
    thread_state_flavor_t flavors[EXC_TYPES_COUNT];
} plcrash_mach_exception_port_state_t;

@interface PLCrashMachExceptionPortState : NSObject {
@private
    /** Exception mask. */
    exception_mask_t _mask;
    
    /** Exception server port. */
    mach_port_t _port;
    
    /** Exception behavior. */
    exception_behavior_t _behavior;
    
    /** Exception thread flavor. */
    thread_state_flavor_t _flavor;
}

- (instancetype) initWithPort: (mach_port_t) port
                         mask: (exception_mask_t) mask
                     behavior: (exception_behavior_t) behavior
                       flavor: (thread_state_flavor_t) flavor;

+ (NSSet *) exceptionPortStatesForTask: (task_t) task mask: (exception_mask_t) mask error: (NSError **) outError;
+ (NSSet *) exceptionPortStatesForThread: (thread_t) thread mask: (exception_mask_t) mask error: (NSError **) outError;

- (BOOL) registerForTask: (task_t) task previousPortStates: (NSSet **) portStates error: (NSError **) outError;
- (BOOL) registerForThread: (thread_t) thread previousPortStates: (NSSet **) portStates error: (NSError **) outError;

/** Exception server port. */
@property(nonatomic, readonly) mach_port_t port;

/** Exception mask. */
@property(nonatomic, readonly) exception_mask_t mask;

/** Exception behavior. */
@property(nonatomic, readonly) exception_behavior_t behavior;

/** Exception thread flavor. */
@property(nonatomic, readonly) thread_state_flavor_t flavor;


@end

#endif /* PLCRASH_FEATURE_MACH_EXCEPTIONS */
