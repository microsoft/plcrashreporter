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

/** @internal
 * Monitored fatal exception types. */
#define FATAL_EXCEPTION_TYPES (EXC_MASK_BAD_ACCESS|EXC_MASK_BAD_INSTRUCTION|EXC_MASK_ARITHMETIC)

// Crash log handler
#if 0
static void dump_crash_log (int signal, siginfo_t *info, ucontext_t *uap);
#endif

@interface PLCrashSignalHandler (PrivateMethods)

- (BOOL) spawnHandlerThreadAndReturnError: (NSError **) outError;

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

- (void) dealloc {
    /* To actually clean up, we'd need to re-register the old exception handler,
     * and then drop our send rights */
    [NSException raise: PLCrashReporterException format: @"The PLCrashSignalHandler process singleton may not be deallocated"];
    [super dealloc];
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
    kern_return_t kr;

    /* Must only be called once per process */
    if (initialized) {
        [NSException raise: PLCrashReporterException format: @"Attempted to re-register per-process signal handler"];
        return NO;
    }
    
    /* Fetch the current exception handler */
    kr = task_get_exception_ports(mach_task_self(),
                                  FATAL_EXCEPTION_TYPES,
                                  _forwardingHandler.masks,
                                  &_forwardingHandler.count,
                                  _forwardingHandler.ports,
                                  _forwardingHandler.behaviors,
                                  _forwardingHandler.flavors);
    if (kr != KERN_SUCCESS) {
        [self populateError: outError machError: kr description: NSLocalizedString(@"Failed to fetch current exception handler", @"")];
        return NO;
    }


    /* Spawn the handler thread */
    if (![self spawnHandlerThreadAndReturnError: outError]) {
        return NO;
    }

    initialized = YES;
    return YES;
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



/**
 * @internal
 * Private Methods
 */
@implementation PLCrashSignalHandler (PrivateMethods)

/**
 * Spawn the thread handler. The thread is started detached, with the minimum reasonable stack
 * size.
 *
 * @warning Do not call more than once.
 */
- (BOOL) spawnHandlerThreadAndReturnError: (NSError **) outError {
    pthread_attr_t attr;
    pthread_t thr;
    BOOL retval = NO;
    
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
    
    /* Success, thread running */
    retval = YES;
    
cleanup:
    /* Clean up */
    pthread_attr_destroy(&attr);
    
    return retval;
}


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