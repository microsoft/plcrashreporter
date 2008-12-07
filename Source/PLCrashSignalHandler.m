/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008 Plausible Labs Cooperative, Inc.
 * All rights reserved.
 */

#import <pthread.h>

#import <mach/mach.h>

#import "PLCrashReporter.h"
#import "PLCrashSignalHandler.h"

// Crash log handler
#if 0
static void dump_crash_log (int signal, siginfo_t *info, ucontext_t *uap);
#endif

@interface PLCrashSignalHandler (PrivateMethods)

- (void) populateError: (NSError **) error
              errnoVal: (int) errnoVal
           description: (NSString *) description;

- (void) populateError: (NSError **) error
             machError: (kern_return_t) machError
           description: (NSString *) description;

- (void) populateError: (NSError **) error 
             errorCode: (PLCrashReporterError) code
           description: (NSString *) description
                 cause: (NSError *) cause;

@end

/** @internal
 * Shared signal handler. Only one may be allocated per process, for obvious reasons. */
static PLCrashSignalHandler *sharedHandler;

/**
 * @internal
 * Background thread that receives and forwards Mach exception handler messages
 */
static void *exception_handler_thread (void *arg) {
    
    return NULL;
}

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



/**
 * Register the Mach exception handlers.
 *
 * @param outError A pointer to an NSError object variable. If an error occurs, this
 * pointer will contain an error object indicating why the signal handlers could not be
 * registered. If no error occurs, this parameter will be left unmodified. You may specify
 * NULL for this parameter, and no error information will be provided. 
 */
- (BOOL) registerHandlerAndReturnError: (NSError **) outError {
    static BOOL initialized = NO;
    pthread_attr_t attr;
    pthread_t thr;
    
    /* Must only be called once per process */
    if (initialized) {
        [NSException raise: PLCrashReporterException format: @"Attempted to re-register per-process signal handler"];
        return NO;
    }
    
    /* Set up the exception handler thread. The thread should be extremely lightweight, as it only
     * exists to handle very unusual conditions.
     *
     * We use a very small stack size (4k or PTHREAD_STACK_MIN, whichever is larger) */
    if (pthread_attr_init(&attr) != 0) {
        [self populateError: outError errnoVal: errno description: NSLocalizedString(@"Could not set pthread stack size", @"")];
        return NO;
    }

    if (pthread_attr_setstacksize(&attr, MAX(PTHREAD_STACK_MIN, 4096)) != 0) {
        [self populateError: outError errnoVal: errno description: NSLocalizedString(@"Could not set pthread stack size", @"")];
        goto cleanup;
    }

    /* start the thread detached */
    if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) != 0) {
        [self populateError: outError errnoVal: errno description: NSLocalizedString(@"Could not set pthread detach state", @"")];
        goto cleanup;
    }
    
    /* Create the exception handler thread */
    if (pthread_create(&thr, &attr, exception_handler_thread, NULL) != 0) {
        [self populateError: outError errnoVal: errno description: NSLocalizedString(@"Could not create exception handler thread", @"")];
        goto cleanup;
    }
    
    /* Success, set initialized */
    initialized = YES;
    
cleanup:
    /* Clean up */
    pthread_attr_destroy(&attr);
    
    return initialized;
}

/**
 * Execute the crash log handler.
 * @warning For testing purposes only.
 *
 * @param signal Signal to emulate.
 * @param code Signal code.
 * @param address Fault address.
 */
- (void) testHandlerWithSignal: (int) signal code: (int) code faultAddress: (void *) address {

}

@end


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
 * Populate an PLCrashReporterErrorOperatingSystem NSError instance, using the provided
 * mach error value to create the underlying error cause.
 */
- (void) populateError: (NSError **) error
             machError: (kern_return_t) machError
           description: (NSString *) description
{
    NSError *cause = [NSError errorWithDomain: NSMachErrorDomain code: machError userInfo: nil];
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

/**
 * Dump the process crash log.
 *
 * XXX Currently outputs to stderr using NSLog, must use the crash log writer
 * implementation (once available)
 */
#if 0
static void dump_crash_log (int signal, siginfo_t *info, ucontext_t *uap) {
    /* Basic signal information */
    NSLog(@"Received signal %d (sig%s) code %d", signal, sys_signame[signal], info->si_code);
    NSLog(@"Signal fault address %p", info->si_addr);    
}
#endif