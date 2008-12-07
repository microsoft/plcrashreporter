/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008 Plausible Labs Cooperative, Inc.
 * All rights reserved.
 */

#import <signal.h>

#import "PLCrashSignalHandler.h"

/**
 * @internal
 * List of monitored fatal signals.
 */
static int fatal_signals[] = {
    SIGABRT,
    SIGBUS,
    SIGFPE,
    SIGFPE,
    SIGILL,
    SIGSEGV
};

/** @internal
 * number of signals in the fatal signals list */
static int n_fatal_signals = (sizeof(fatal_signals) / sizeof(fatal_signals[0]));


/** @internal
 * Root fatal signal handler */
static void fatal_signal_handler (int signal, siginfo_t *info, void *uapVoid) {
    // todo

    /* Re-raise the signal */
    raise(signal);
}

/**
 * @internal
 * Shared signal handler. Only one may be allocated per process, for obvious reasons.
 */
static PLCrashSignalHandler *sharedHandler;

/***
 * @internal
 * Implements Crash Reporter signal handling. Singleton.
 */
@implementation PLCrashSignalHandler

/* Set up the singleton signal handler */
+ (void) initialize {
    sharedHandler = [[PLCrashSignalHandler alloc] init];
}

/**
 * Return the process signal handler. Only one signal handler may be registered per process.
 */
+ (PLCrashSignalHandler *) sharedHandler {
    if (sharedHandler == nil)
        sharedHandler = [[PLCrashSignalHandler alloc] init];

    return sharedHandler;
}

- (BOOL) registerHandlerForSignal: (int) signal error: (NSError **) outError {
    struct sigaction new_action;

    /* Configure action */
    new_action.sa_flags = SA_SIGINFO;
    sigemptyset(&new_action.sa_mask);
    new_action.sa_sigaction = &fatal_signal_handler;

    /* Set new sigaction */
    if (sigaction(signal, &new_action, NULL) != 0) {
        int err = errno;
        if (outError)
            *outError = [NSError errorWithDomain: NSPOSIXErrorDomain code: errno userInfo: nil];

        NSLog(@"Signal registration for %s failed: %s", strsignal(signal), strerror(err));
        return NO;
    }

    return YES;
}

/**
 * Register the process signal handlers. May be called more than once, but will
 * simply reset previously registered signal handlers.
 *
 * @note If this method returns NO, some signal handlers may have been registered
 * successfully.
 *
 * @param outError A pointer to an NSError object variable. If an error occurs, this
 * pointer will contain an error object indicating why the signal handlers could not be
 * registered. If no error occurs, this parameter will be left unmodified. You may specify
 * NULL for this parameter, and no error information will be provided. 
 */
- (BOOL) registerHandlerAndReturnError: (NSError **) outError {
    /* Register handler for signals */
    for (int i = 0; i < n_fatal_signals; i++) {
        if (![self registerHandlerForSignal: fatal_signals[i] error: outError])
            return NO;
    }

    return YES;
}

@end
