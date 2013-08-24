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
 * Manages the internal state for a user-registered callback and context.
 */
struct plcrash_signal_user_callback {
    /** Signal handler callback function. */
    PLCrashSignalHandlerCallbackFunc callback;
    
    /** Signal handler context. */
    void *context;
};

/**
 * @internal
 *
 * A signal handler callback context.
 */
struct PLCrashSignalHandlerCallback {
    /**
     * Internal callback function. This function is responsible for determining the next
     * signal handler in the chain of handlers, and issueing the actual PLCrashSignalHandlerCallback()
     * invocation.
     */
    bool (*callback)(int signo, siginfo_t *info, ucontext_t *uap, void *context);

    /** Signal handler context. */
    void *context;
};

/**
 * @internal
 *
 * A registered POSIX signal handler action. This is used to represent previously
 * registered signal handlers that have been replaced by the PLCrashSignalHandler's
 * global signal handler.
 */
struct plcrash_signal_handler_action {
    /** Signal type. */
    int signo;
    
    /** Signal handler action. */
    struct sigaction action;
};

/**
 * Signal handler context that must be global for async-safe
 * access.
 */
static struct {
    /** @internal
     * Registered callbacks. */
    async_list<plcrash_signal_user_callback> callbacks;
    
    /** @internal
     * Originaly registered signal handlers */
    async_list<plcrash_signal_handler_action> previous_actions;
} shared_handler_context;

/*
 * Finds and executes the first matching signal handler in the shared previous_actions list; this is used
 * to support executing process-wide POSIX signal handlers that were previously registered before being replaced by
 * PLCrashSignalHandler::registerHandlerForSignal:.
 */
static bool previous_action_callback (int signo, siginfo_t *info, ucontext_t *uap, void *context, PLCrashSignalHandlerCallback *next) {
    bool handled = false;

    /* Let any additional handler execute */
    if (PLCrashSignalHandlerForward(next, signo, info, uap))
        return true;

    shared_handler_context.previous_actions.set_reading(true); {
        /* Find the first matching handler */
        async_list<plcrash_signal_handler_action>::node *next = NULL;
        while ((next = shared_handler_context.previous_actions.next(next)) != NULL) {
            /* Skip non-matching entries */
            if (next->value().signo != signo)
                continue;

            /* Found a match */
            // TODO - Should we handle the other flags, eg, SA_RESETHAND, SA_ONSTACK? */
            if (next->value().action.sa_flags & SA_SIGINFO) {
                next->value().action.sa_sigaction(signo, info, (void *) uap);
                handled = true;
            } else {
                switch ((uintptr_t) (next->value().action.sa_handler)) {
                    case ((uintptr_t) SIG_IGN):
                        /* Ignored */
                        handled = true;
                        break;
                        
                    case ((uintptr_t) SIG_DFL):
                        /* Default handler should be run, be we have no mechanism to pass through to
                         * the default handler; mark the signal as unhandled. */
                        handled = false;
                        break;
                        
                    default:
                        /* Handler registered, execute it */
                        next->value().action.sa_handler(signo);
                        handled = true;
                        break;
                }
            }
            
            /* Handler was found; iteration done */
            break;
        }
    } shared_handler_context.previous_actions.set_reading(false);

    return handled;
}

/*
 * Recursively iterates the actual callbacks registered in our shared_handler_context. To begin iteration,
 * provide a value of NULL for 'context'.
 */
static bool internal_callback_iterator (int signo, siginfo_t *info, ucontext_t *uap, void *context) {
    /* Call the next handler in the chain. If this is the last handler in the chain, pass it the original signal
     * handlers. */
    bool handled = false;
    shared_handler_context.callbacks.set_reading(true); {
        async_list<plcrash_signal_user_callback>::node *prev = (async_list<plcrash_signal_user_callback>::node *) context;
        async_list<plcrash_signal_user_callback>::node *current = shared_handler_context.callbacks.next(prev);

        /* Check for end-of-list */
        if (current == NULL) {
            shared_handler_context.callbacks.set_reading(false);
            return false;
        }
        
        /* Check if any additional handlers are registered. If so, provide the next handler as the forwarding target. */
        if (shared_handler_context.callbacks.next(current) != NULL) {
            PLCrashSignalHandlerCallback next_handler = {
                .callback = internal_callback_iterator,
                .context = current
            };
            handled = current->value().callback(signo, info, uap, current->value().context, &next_handler);
        } else {
            /* Otherwise, we've hit the final handler in the list. */
            handled = current->value().callback(signo, info, uap, current->value().context, NULL);
        }
    } shared_handler_context.callbacks.set_reading(false);

    return handled;
};

/** 
 * @internal
 *
 * The signal handler function used by PLCrashSignalHandler. This function should not be called or referenced directly,
 * but is exposed to allow simulating signal handling behavior from unit tests.
 *
 * @param signo The signal number.
 * @param info The signal information.
 * @param uapVoid A ucontext_t pointer argument.
 */
void plcrash_signal_handler (int signo, siginfo_t *info, void *uapVoid) {
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
    if (!internal_callback_iterator(signo, info, (ucontext_t *) uapVoid, NULL))
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
    if (next == NULL)
        return false;

    return next->callback(sig, info, uap, next->context);
}

/***
 * @internal
 *
 * Manages a process-wide signal handler, including async-safe registration of multiple callbacks, and pass-through
 * to previously registered signal handlers.
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
 * Register all process-wide signal handlers, if not yet registered. We register the actual signal handlers just once across all instances,
 * though any number of PLCrashSignalHandler instances may be instantiated. All instances share the same async-safe/thread-safe
 * ordered list of callbacks.
 *
 * @param outError A pointer to an NSError object variable. If an error occurs, this
 * pointer will contain an error object indicating why the signal handlers could not be
 * registered. If no error occurs, this parameter will be left unmodified.
 *
 * @note If this method returns NO, some signal handlers may have been registered
 * successfully.
 */
- (BOOL) registerSignalHandlers: (NSError **) outError {
    static pthread_mutex_t registerHandlers = PTHREAD_MUTEX_INITIALIZER;
    static BOOL handlersRegistered = NO;
    pthread_mutex_lock(&registerHandlers); {
        /* If already registered, nothing to do */
        if (handlersRegistered) {
            pthread_mutex_unlock(&registerHandlers);
            return YES;
        }
        
        /* Register our signal stack */
        if (sigaltstack(&_sigstk, 0) < 0) {
            /* This should only fail if we supply invalid arguments to sigaltstack() */
            plcrash_populate_posix_error(outError, errno, @"Could not initialize alternative signal stack");
            return NO;
        }
        
        /* Add the pass-through sigaction callback as the last element in the callback list. */
        plcrash_signal_user_callback sa = {
            .callback = previous_action_callback,
            .context = NULL
        };
        shared_handler_context.callbacks.nasync_append(sa);

        /* Register handler for signals */
        for (int i = 0; i < n_fatal_signals; i++) {
            struct sigaction sa;
            struct sigaction sa_prev;

            /* Configure action */
            memset(&sa, 0, sizeof(sa));
            sa.sa_flags = SA_SIGINFO|SA_ONSTACK;
            sigemptyset(&sa.sa_mask);
            sa.sa_sigaction = &plcrash_signal_handler;
            
            /* Set new sigaction */
            if (sigaction(fatal_signals[i], &sa, &sa_prev) != 0) {
                int err = errno;
                plcrash_populate_posix_error(outError, err, @"Failed to register signal handler");
                NSLog(@"Signal registration for %s failed: %s", strsignal(fatal_signals[i]), strerror(err));
                return NO;
            }
            
            /* Save the previous action. Note that there's an inescapable race condition here, such that
             * we may not call the previous signal handler if signal occurs prior to our saving
             * the caller's handler. */
            plcrash_signal_handler_action act = {
                .signo = fatal_signals[i],
                .action = sa_prev
            };
            shared_handler_context.previous_actions.nasync_append(act);
        }
    } pthread_mutex_unlock(&registerHandlers);
    
    return YES;
}

/**
 * Register a signal handler with the provided callback function.
 *
 * @param crashCallback Callback called upon receipt of a signal. The callback will execute on the crashed
 * thread, using an alternate, limited stack.
 * @param context Context to be passed to the callback. May be NULL.
 * @param outError A pointer to an NSError object variable. If an error occurs, this
 * pointer will contain an error object indicating why the signal handlers could not be
 * registered. If no error occurs, this parameter will be left unmodified. You may specify
 * NULL for this parameter, and no error information will be provided. 
 *
 * @note If this method returns NO, some process-wide signal handlers may have been registered
 * successfully.
 */
- (BOOL) registerHandlerWithCallback: (PLCrashSignalHandlerCallbackFunc) crashCallback context: (void *) context error: (NSError **) outError {
    /* Register the actual signal handlers, if necessary */
    if (![self registerSignalHandlers: outError])
        return NO;

    /* Add the new callback to the shared state list. */
    plcrash_signal_user_callback reg = {
        .callback = crashCallback,
        .context = context
    };
    shared_handler_context.callbacks.nasync_prepend(reg);

    return YES;
}

@end
