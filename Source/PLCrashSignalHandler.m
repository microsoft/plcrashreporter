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

// OS-supplied mach exception server implementation
extern boolean_t exc_server(mach_msg_header_t *, mach_msg_header_t *);

/** @internal
 * Monitored fatal exception types. */
#define FATAL_EXCEPTION_TYPES (EXC_MASK_BAD_ACCESS|EXC_MASK_BAD_INSTRUCTION|EXC_MASK_ARITHMETIC)

// Crash log handler
#if 0
static void dump_crash_log (int signal, siginfo_t *info, ucontext_t *uap);
#endif


@interface PLCrashSignalHandler (PrivateMethods)

static void *exception_handler_thread (void *arg);

- (BOOL) saveExceptionHandler: (PLCrashSignalHandlerPorts *) saved error: (NSError **) outError;
- (BOOL) registerHandlerPort: (mach_port_t *) exceptionPort error: (NSError **) outError;
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

    /* Must only be called once per process */
    if (initialized) {
        [NSException raise: PLCrashReporterException format: @"Attempted to re-register per-process signal handler"];
        return NO;
    }
    
    /* Fetch the current exception handler */
    if (![self saveExceptionHandler: &_forwardingHandler error: outError]) {
        return NO;
    }

    /* Register exception handler ports */
    if (![self registerHandlerPort: &_exceptionPort error: outError]) {
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
 * Background thread that receives and forwards Mach exception handler messages
 */
static void *exception_handler_thread (void *arg) {
    sigset_t sigset;
    PLCrashSignalHandler *self = arg;
    // PLCrashSignalHandlerPorts *oldHandler = &self->_forwardingHandler;

    /* Block all signals */
    sigfillset(&sigset);
    if (pthread_sigmask(SIG_BLOCK, &sigset, NULL) != 0)
        NSLog(@"Warning - could not block signals on Mach exception handler thread: %s", strerror(errno));

    /* Run forever */
    mach_msg_server(exc_server, 2048, self->_exceptionPort, 0);
    
    return NULL;
}


/**
 * Called by mach_msg_server.
 */
kern_return_t catch_exception_raise(mach_port_t exception_port,
                                           mach_port_t thread,
                                           mach_port_t task,
                                           exception_type_t exception,
                                           exception_data_t code_vector,
                                           mach_msg_type_number_t code_count)
{
    const char hello[] = "Hello\n";
    write(STDERR_FILENO, hello, sizeof(hello));
    return KERN_SUCCESS;
}

/**
 * Save the current exception handler.
 */
- (BOOL) saveExceptionHandler: (PLCrashSignalHandlerPorts *) saved error: (NSError **) outError {
    kern_return_t kr;
    kr = task_get_exception_ports(mach_task_self(),
                                  FATAL_EXCEPTION_TYPES,
                                  saved->masks,
                                  &saved->count,
                                  saved->ports,
                                  saved->behaviors,
                                  saved->flavors);
    if (kr != KERN_SUCCESS) {
        [self populateError: outError machError: kr description: @"Failed to fetch current exception handler"];
        return NO;
    }

    return YES;
}


/**
 * Register the Mach exception handler. On success, the exceptionPort argument will
 * be populated with the new exception port. It is the caller's responsibility to release
 * this port.
 */
- (BOOL) registerHandlerPort: (mach_port_t *) exceptionPort error: (NSError **) outError {
    kern_return_t kr;
    mach_port_t task = mach_task_self();    

    /* Allocate the receive right */
    kr = mach_port_allocate(task, MACH_PORT_RIGHT_RECEIVE, exceptionPort);
    if (kr != KERN_SUCCESS) {
        [self populateError: outError machError: kr description: @"Could not create mach exception port"];
        return NO;
    }

    /* Insert a send right */
    kr = mach_port_insert_right(task, *exceptionPort, *exceptionPort, MACH_MSG_TYPE_MAKE_SEND);
    if (kr != KERN_SUCCESS) {
        [self populateError: outError machError: kr description: @"Could not insert send right for mach exception port"];
        goto error;
    }

    /* Set the task exception ports */
    kr = task_set_exception_ports(task,
                                  FATAL_EXCEPTION_TYPES,
                                  *exceptionPort,
                                  EXCEPTION_DEFAULT,
                                  MACHINE_THREAD_STATE);
    if (kr != KERN_SUCCESS) {
        [self populateError: outError machError: kr description: @"Could not set task exception port"];
        goto error;
    }

    return YES;

error:
    mach_port_deallocate(task, *exceptionPort);
    return NO;
}


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
        [self populateError: outError errnoVal: errno description: @"Could not set pthread stack size"];
        return NO;
    }
    
    if (pthread_attr_setstacksize(&attr, MAX(PTHREAD_STACK_MIN, 4096)) != 0) {
        [self populateError: outError errnoVal: errno description: @"Could not set pthread stack size"];
        goto cleanup;
    }
    
    /* start the thread detached */
    if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) != 0) {
        [self populateError: outError errnoVal: errno description: @"Could not set pthread detach state"];
        goto cleanup;
    }
    
    /* Create the exception handler thread */
    if (pthread_create(&thr, &attr, exception_handler_thread, self) != 0) {
        [self populateError: outError errnoVal: errno description: @"Could not create exception handler thread"];
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