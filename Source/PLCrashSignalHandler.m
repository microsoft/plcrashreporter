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

#import <signal.h>
#import <unistd.h>

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
 * Signal handler context that must be global for async-safe
 * access.
 */
static struct {
    /** @internal
     * Initialization flag. Set to YES if the signal handler has been registered. */
    BOOL handlerRegistered;

    /** @internal
     * Shared signal handler. Only one may be allocated per process, for obvious reasons. */
    PLCrashSignalHandler *sharedHandler;
    
    /** @internal
     * Crash signal callback. */
    PLCrashSignalHandlerCallback crashCallback;
    
    /** @internal
     * Crash signal callback context */
    void *crashCallbackContext;
} SharedHandlerContext = {
    .handlerRegistered = NO,
    .sharedHandler = nil,
    .crashCallback = NULL,
    .crashCallbackContext = NULL
};


/** @internal
 * Root fatal signal handler */
static void fatal_signal_handler (int signal, siginfo_t *info, void *uapVoid) {
    /* Remove all signal handlers -- if the dump code fails, the default terminate
     * action will occur */
    for (int i = 0; i < n_fatal_signals; i++) {
        struct sigaction sa;
        
        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = SIG_DFL;
        sigemptyset(&sa.sa_mask);
        
        sigaction(fatal_signals[i], &sa, NULL);
    }

    /* Call the callback handler */
    if (SharedHandlerContext.crashCallback != NULL)
        SharedHandlerContext.crashCallback(signal, info, uapVoid, SharedHandlerContext.crashCallbackContext);
    
    /* Re-raise the signal */
    raise(signal);
}


@interface PLCrashSignalHandler (PrivateMethods)

- (void) populateError: (NSError **) error
              errnoVal: (int) errnoVal
           description: (NSString *) description;

- (void) populateError: (NSError **) error 
             errorCode: (PLCrashReporterError) code
           description: (NSString *) description
                 cause: (NSError *) cause;

@end


/***
 * @internal
 * Implements Crash Reporter signal handling. Singleton.
 */
@implementation PLCrashSignalHandler

/**
 * Return the process signal handler. Only one signal handler may be registered per process.
 */
+ (PLCrashSignalHandler *) sharedHandler {
    /* Initialize the context, if required */
    if (SharedHandlerContext.sharedHandler == NULL) {
        memset(&SharedHandlerContext, 0, sizeof(SharedHandlerContext));
        SharedHandlerContext.sharedHandler = [[PLCrashSignalHandler alloc] init];
    }

    return SharedHandlerContext.sharedHandler;
}

/**
 * @internal
 *
 * Initialize the signal handler. API clients should use he +[PLCrashSignalHandler sharedHandler] method.
 *
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
        if (outError)
            *outError = [NSError errorWithDomain: NSPOSIXErrorDomain code: errno userInfo: nil];
        
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
- (BOOL) registerHandlerWithCallback: (PLCrashSignalHandlerCallback) crashCallback context: (void *) context error: (NSError **) outError {
    /* Prevent duplicate registrations */
    if (SharedHandlerContext.handlerRegistered)
        [NSException raise: PLCrashReporterException format: @"Signal handler has already been registered"];
    SharedHandlerContext.handlerRegistered = YES;

    /* Save the callback function */
    SharedHandlerContext.crashCallback = crashCallback;
    SharedHandlerContext.crashCallbackContext = context;

    /* Register our signal stack */
    if (sigaltstack(&_sigstk, 0) < 0) {
        [self populateError: outError errnoVal: errno description: @"Could not initialize alternative signal stack"];
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



/**
 * @internal
 * Private Methods
 */
@implementation PLCrashSignalHandler (PrivateMethods)

/**
 * Populate an PLCrashReporterErrorOperatingSystem NSError instance, using the provided
 * errno error value to create the underlying error cause.
 */
- (void) populateError: (NSError **) error
              errnoVal: (int) errnoVal
           description: (NSString *) description
{
    NSError *cause = [NSError errorWithDomain: NSPOSIXErrorDomain code: errnoVal userInfo: nil];
    [self populateError: error errorCode: PLCrashReporterErrorOperatingSystem description: description cause: cause];
}


/**
 * Populate an NSError instance with the provided information.
 *
 * @param error Error instance to populate. If NULL, this method returns
 * and nothing is modified.
 * @param code The error code corresponding to this error.
 * @param description A localized error description.
 * @param cause The underlying cause, if any. May be nil.
 */
- (void) populateError: (NSError **) error 
             errorCode: (PLCrashReporterError) code
           description: (NSString *) description
                 cause: (NSError *) cause
{
    NSMutableDictionary *userInfo;
    
    if (error == NULL)
        return;

    /* Create the userInfo dictionary */
    userInfo = [NSMutableDictionary dictionaryWithObjectsAndKeys:
                description, NSLocalizedDescriptionKey,
                nil
                ];

    /* Add the cause, if available */
    if (cause != nil)
        [userInfo setObject: cause forKey: NSUnderlyingErrorKey];
    
    *error = [NSError errorWithDomain: PLCrashReporterErrorDomain code: code userInfo: userInfo];
}


@end
