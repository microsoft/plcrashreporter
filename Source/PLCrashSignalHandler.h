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

#import <Foundation/Foundation.h>

typedef struct plcrash_signal_handler_callback_set plcrash_signal_handler_callback_set_t;

/**
 * @internal
 * Signal handler callback.
 *
 * @param signal The received signal.
 * @param info The signal info.
 * @param uap The signal thread context.
 * @param next The next set of signal handlers. May be used to forward the signal via PLCrashSignalHandlerForward().
 * @param context The previously specified context for this handler.
 *
 * @return Return true if the signal was handled and execution should continue, false if the signal was not handled.
 */
typedef bool (*PLCrashSignalHandlerCallback)(int signal, siginfo_t *info, ucontext_t *uap, plcrash_signal_handler_callback_set_t *next, void *context);

/**
 * @internal
 *
 * A set of PLCrashSignalHandlerCallback and the signals/contexts for which they are registered.
 *
 * Up to NSIG entries may be returned. The actual count is provided via plcrash_signal_handler_callback_set::count.
 * The values stored in the arrays correspond positionally.
 */
struct plcrash_signal_handler_callback_set {
    /** Number of independent signal handler sets (up to NSIG). */
    int count;

    /** Signal types. */
    int signals[NSIG];

    /** Signal handler functions. */
    PLCrashSignalHandlerCallback funcs[NSIG];

    /** Signal handler contexts. */
    void *contexts[NSIG];
};


bool PLCrashSignalHandlerForward (int signal, siginfo_t *info, ucontext_t *uap, plcrash_signal_handler_callback_set_t *callback_set);

@interface PLCrashSignalHandler : NSObject {
@private
    /** Signal stack */
    stack_t _sigstk;
}


+ (PLCrashSignalHandler *) sharedHandler;
- (BOOL) registerHandlerWithCallback: (PLCrashSignalHandlerCallback) crashCallback context: (void *) context error: (NSError **) outError;

@end
