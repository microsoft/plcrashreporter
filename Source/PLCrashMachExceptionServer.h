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

#import "PLCrashReporterBuildConfig.h"
#import "PLCrashMachExceptionPortState.h"

#if PLCRASH_FEATURE_MACH_EXCEPTIONS

/**
 * @internal
 * Exception handler callback.
 *
 * @param task The task in which the exception occured.
 * @param thread The thread on which the exception occured. The thread will be suspended when the callback is issued, and may be resumed
 * by the callback using thread_resume().
 * @param exception_type Mach exception type.
 * @param code Mach exception codes.
 * @param code_count The number of codes provided.
 * @param context The context supplied to PLCrashMachExceptionServer::registerHandlerForTask:withCallback:context:error
 *
 * @return Return KERN_SUCCESS if the exception has been handled. Return an appropriate failure code otherwise. If KERN_SUCCESS, the thread
 * will be resumed.
 */
typedef kern_return_t (*PLCrashMachExceptionHandlerCallback) (task_t task,
                                                              thread_t thread,
                                                              exception_type_t exception_type,
                                                              mach_exception_data_t code,
                                                              mach_msg_type_number_t code_count,
                                                              void *context);

kern_return_t PLCrashMachExceptionForward (task_t task,
                                           thread_t thread,
                                           exception_type_t exception_type,
                                           mach_exception_data_t code,
                                           mach_msg_type_number_t code_count,
                                           plcrash_mach_exception_port_state_t *port_state);

@interface PLCrashMachExceptionServer : NSObject {
@private
    /** Backing server context. This structure will not be allocated until the background
     * exception server thread is spawned; once the server thread has been successfully started,
     * it is that server thread's responsibility to deallocate this structure. */
    struct plcrash_exception_server_context *_serverContext;
}

- (id) initWithCallBack: (PLCrashMachExceptionHandlerCallback) callback
                context: (void *) context
                  error: (NSError **) outError;

- (BOOL) registerForTask: (task_t) task mask: (exception_mask_t) mask previousPortStates: (NSSet **) portStates error: (NSError **) outError;
- (BOOL) registerForThread: (thread_t) thread mask: (exception_mask_t) mask previousPortStates: (NSSet **) portStates error: (NSError **) outError;

/** The Mach thread on which the exception server is running. This may be used to register
 * a thread-specific exception handler for the server itself. */
@property(nonatomic, readonly) thread_t serverThread;

@end

#endif /* PLCRASH_FEATURE_MACH_EXCEPTIONS */
