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

#import "CrashReporter.h"
#import "PLCrashAsync.h"
#import "PLCrashSignalHandler.h"
#import "PLCrashFrameWalker.h"
#import "PLCrashReporterNSError.h"

#import "PLCrashAsyncLinkedList.hpp"

#import <signal.h>
#import <unistd.h>

using namespace plcrash::async;

/**
 * @internal
 * List of monitored fatal signals.
 */
static int fatal_signals[] = {
    SIGABRT,
    SIGBUS,
    SIGFPE,
    SIGILL,
    SIGSEGV,
    SIGTRAP
};

/** @internal
 * number of signals in the fatal signals list */
static int n_fatal_signals = (sizeof(fatal_signals) / sizeof(fatal_signals[0]));

/**
 * @internal
 *
 * PLCrashSignalHandlerCallbackFunc and associated context.
 */
struct PLCrashSignalHandlerCallback {
    /** Signal handler callback function. */
    PLCrashSignalHandlerCallbackFunc callback;
    
    /** Signal handler context. */
    void *context;
};

/**
 * @internal
 *
 * A set of registered signal handlers.
 *
 * Up to NSIG entries may be returned. The actual count is provided via plcrash_signal_handler_set::count.
 * The values stored in the arrays correspond positionally.
 */
struct plcrash_signal_handler_set {
    /** Number of independent signal handler sets (up to NSIG). */
    uint32_t count;
    
    /** Signal types. */
    int signals[NSIG];
    
    /** Signal handler actions. */
    struct sigaction actions[NSIG];
};

/**
 * Signal handler context that must be global for async-safe
 * access.
 */
static struct {
    /** @internal
     * Registered callbacks. */
    async_list<PLCrashSignalHandlerCallback> registered_handlers;
    
    /** @internal
     * Originaly registered signal handlers */
    plcrash_signal_handler_set previous_handlers;
} shared_handler_context;

/*
 * A signal handler callback used to recursively iterate the actual callbacks registered in our shared_handler_context. To begin iteration,
 * provide a value of NULL for 'next'.
 */
static bool internal_callback_iterator (int signo, siginfo_t *info, ucontext_t *uap, void *context, PLCrashSignalHandlerCallback *next) {
    /* Call the next handler in the chain. If this is the last handler in the chain, pass it the original signal
     * handlers. */
    bool handled = false;
    shared_handler_context.registered_handlers.set_reading(true); {
        async_list<PLCrashSignalHandlerCallback>::node *prev = (async_list<PLCrashSignalHandlerCallback>::node *) context;
        async_list<PLCrashSignalHandlerCallback>::node *current = shared_handler_context.registered_handlers.next(prev);
        
        /* Check if any additional handlers are registered. If so, provide the next handler as the forwarding target. */
        if (shared_handler_context.registered_handlers.next(current) != NULL) {
            PLCrashSignalHandlerCallback next_handler = {
                .callback = internal_callback_iterator,
                .context = current
            };
            handled = current->value().callback(signo, info, uap, current->value().context, &next_handler);
        } else {
            /* Otherwise, we've hit the final handler in the list. */
            handled = current->value().callback(signo, info, uap, current->value().context, NULL);
        }
    } shared_handler_context.registered_handlers.set_reading(false);

    return handled;
};

/** @internal
 * Root fatal signal handler */
static void fatal_signal_handler (int signo, siginfo_t *info, void *uapVoid) {
    /* Remove all signal handlers -- if the dump code fails, the default terminate
     * action will occur.
     *
     * NOTE: SA_RESETHAND breaks SA_SIGINFO on ARM, so we reset the handlers manually.
     * http://openradar.appspot.com/11839803
     *
     * TODO: When forwarding signals (eg, to Mono's runtime), resetting the signal handlers
     * could result in incorrect runtime behavior; we should revisit resetting the
     * signal handlers once we address double-fault handling.
     */
    for (int i = 0; i < n_fatal_signals; i++) {
        struct sigaction sa;
        
        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = SIG_DFL;
        sigemptyset(&sa.sa_mask);
        
        sigaction(fatal_signals[i], &sa, NULL);
    }
    
    /* Start iteration; we currently re-raise the signal if not handled by callbacks; this should be revisited
     * in the future, as the signal may not be raised on the expected thread.
     */
    if (!internal_callback_iterator(signo, info, (ucontext_t *) uapVoid, NULL, NULL))
        raise(signo);
}

/**
 * Forward a signal to the first matching callback in @a next, if any.
 *
 * @param next The signal handler callback to which the signal should be forwarded. This value may be NULL,
 * in which case false will be returned.
 * @param sig The signal number.
 * @param info The signal info.
 * @param uap The signal thread context.
 *
 * @return Returns true if the exception was handled by a registered signal handler, or false
 * if the exception was not handled, or no signal handler was registered for @a signo.
 *
 * @note This function is async-safe.
 */
bool PLCrashSignalHandlerForward (PLCrashSignalHandlerCallback *next, int sig, siginfo_t *info, ucontext_t *uap) {
    // TODO
    return false;
}

/***
 * @internal
 * Implements Crash Reporter signal handling. Singleton.
 */
@implementation PLCrashSignalHandler

/* Shared signal handler. Since signal handlers are process-global, it would be unusual
 * for more than one instance to be required. */
static PLCrashSignalHandler *sharedHandler;

+ (void) initialize {
    if ([self class] != [PLCrashSignalHandler class])
        return;
    
    sharedHandler = [[self alloc] init];
}

/**
 * Return the shared signal handler.
 */
+ (PLCrashSignalHandler *) sharedHandler {
    return sharedHandler;
}

/**
 * Initialize a new signal handler instance.
 *
 * API clients should generally prefer the +[PLCrashSignalHandler sharedHandler] method.
 */
- (id) init {
    if ((self = [super init]) == nil)
        return nil;
    
    /* Set up an alternate signal stack for crash dumps. Only 64k is reserved, and the
     * crash dump path must be sparing in its use of stack space. */
    _sigstk.ss_size = MAX(MINSIGSTKSZ, 64 * 1024);
    _sigstk.ss_sp = malloc(_sigstk.ss_size);
    _sigstk.ss_flags = 0;

    /* (Unlikely) malloc failure */
    if (_sigstk.ss_sp == NULL) {
        [self release];
        return nil;
    }

    return self;
}

/**
 * Register the the signal handler for the given signal.
 *
 * @param outError A pointer to an NSError object variable. If an error occurs, this
 * pointer will contain an error object indicating why the signal handlers could not be
 * registered. If no error occurs, this parameter will be left unmodified. You may specify
 * NULL for this parameter, and no error information will be provided. 
 */
- (BOOL) registerHandlerForSignal: (int) signal error: (NSError **) outError {
    struct sigaction sa;
    
    /* Configure action */
    memset(&sa, 0, sizeof(sa));
    sa.sa_flags = SA_SIGINFO|SA_ONSTACK;
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = &fatal_signal_handler;
    
    /* Set new sigaction */
    if (sigaction(signal, &sa, NULL) != 0) {
        int err = errno;
        plcrash_populate_posix_error(outError, err, @"Failed to register signal handler");        
        NSLog(@"Signal registration for %s failed: %s", strsignal(signal), strerror(err));
        return NO;
    }
    
    return YES;
}

/**
 * Register the process signal handlers with the provided callback.
 * Should not be called more than once.
 *
 * @note If this method returns NO, some signal handlers may have been registered
 * successfully.
 *
 * @param crashCallback Callback called upon receipt of a signal. The callback will execute on the crashed
 * thread, using an alternate, limited stack.
 * @param context Context to be passed to the callback. May be NULL.
 * @param outError A pointer to an NSError object variable. If an error occurs, this
 * pointer will contain an error object indicating why the signal handlers could not be
 * registered. If no error occurs, this parameter will be left unmodified. You may specify
 * NULL for this parameter, and no error information will be provided. 
 */
- (BOOL) registerHandlerWithCallback: (PLCrashSignalHandlerCallbackFunc) crashCallback context: (void *) context error: (NSError **) outError {
    /* TODO - we should register the actual signal handlers just once, though any number of PLCrashSignalHandler instances may be 
     * enabled. */
#if 0
    /* Prevent duplicate registrations */
    if (SharedHandlerContext.handlerRegistered)
        [NSException raise: PLCrashReporterException format: @"Signal handler has already been registered"];
    SharedHandlerContext.handlerRegistered = YES;
#endif

    /* Save the callback function in the shared state list. */
    PLCrashSignalHandlerCallback reg = {
        .callback = crashCallback,
        .context = context
    };
    shared_handler_context.registered_handlers.nasync_prepend(reg);

    /* Register our signal stack */
    if (sigaltstack(&_sigstk, 0) < 0) {
        plcrash_populate_posix_error(outError, errno, @"Could not initialize alternative signal stack");
        return NO;
    }

    /* Register handler for signals */
    for (int i = 0; i < n_fatal_signals; i++) {
        if (![self registerHandlerForSignal: fatal_signals[i] error: outError])
            return NO;
    }

    return YES;
}

@end
