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
 * @param double_fault If true, the callback is being called from a double-fault handler. This occurs when
 * your callback -- or the exception server itself -- crashes during handling. Triple faults are not handled, and will
 * simply trigger the OS crash reporter.
 * @param context The context supplied to PLCrashMachExceptionServer::registerHandlerForTask:withCallback:context:error
 *
 * @return Return true if the exception has been handled. Return false otherwise. If true, the thread
 * will be resumed.
 *
 * @par Double Faults 
 *
 * In the case of a double fault, it is valuable to be able to detect that the crash reporter itself crashed,
 * and if possible, provide debugging information that can be reported to our upstream project.
 *
 * How this is handled depends on whether you are running in-process, or out-of-process.
 *
 * @par Out-of-process
 * In the case that the reporter is running out-of-process, it is recommended that you write a
 * cookie to disk to track that the reporter itself failed, and then actually use a crash reporter
 * *on your crash reporter* to report the crash.
 *
 * Yes, "Yo dawg, I heard you like crash reporters ...". It's less likely that a crash handling a user's
 * process is likely to *also* crash the crash reporting process.
 *
 * @par In-process
 *
 * When running in-process (ie, on iOS), it is far more likely that re-running the crash reporter
 * will trigger the same crash again. Thus, it is recommended that an implementor handle double
 * faults in a "safe mode" less likely to trigger an additional crash, and gauranteed to record
 * (at a minimum) that the crash report itself crashed, even if no additional crash data can be
 * recorded.
 *
 * An example implementation might do the following:
 * - Before performing any other operations, create a cookie file on-disk that can be checked on
 *   startup to determine whether the crash reporter itself crashed. This at the very least will
 *   let users know that a problem exists.
 * - Re-run the crash report writer, disabling any risky code paths that are not strictly necessary, e.g.:
 *     - Disable local symbolication if it has been enabled by the user. This will avoid
 *       a great deal if binary parsing.
 *     - Disable reporting on any threads other than the crashed thread. This will avoid
 *       any bugs that may have occured in the stack unwinding code for existing threads:
 */
typedef bool (*PLCrashMachExceptionHandlerCallback) (task_t task,
                                                     thread_t thread,
                                                     exception_type_t exception_type,
                                                     mach_exception_data_t code,
                                                     mach_msg_type_number_t code_count,
                                                     bool double_fault,
                                                     void *context);

@interface PLCrashMachExceptionServer : NSObject {
@private
    /** Backing server context. This structure will not be allocated until the background
     * exception server thread is spawned; once the server thread has been successfully started,
     * it is that server thread's responsibility to deallocate this structure. */
    struct plcrash_exception_server_context *_serverContext;
}

- (BOOL) registerHandlerForTask: (task_t) task
                         thread: (thread_t) thread
                   withCallback: (PLCrashMachExceptionHandlerCallback) callback
                        context: (void *) context
                          error: (NSError **) outError;

- (BOOL) deregisterHandlerAndReturnError: (NSError **) outError;

@end
