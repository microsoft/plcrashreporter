/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008 Plausible Labs Cooperative, Inc.
 * All rights reserved.
 */

#import <pthread.h>
#import <unistd.h>

#import <mach/mach.h>

#import "mig/exc.h"
#import "mach_exc.h"

#import "PLCrashReporter.h"
#import "PLCrashSignalHandler.h"

// A super simple debug 'function' that's async safe.
#define ASYNC_DEBUG(msg) {\
    const char output[] = msg "\n";\
    write(STDERR_FILENO, output, sizeof(output));\
}

// YES if the process shared instance has been initialized
static BOOL initialized = NO;

// mach exception server implementation
extern boolean_t plexc_server(mach_msg_header_t *, mach_msg_header_t *);

/** @internal
 * Monitored fatal exception types. */
#define FATAL_EXCEPTION_TYPES (EXC_MASK_BAD_ACCESS|EXC_MASK_BAD_INSTRUCTION|EXC_MASK_ARITHMETIC)

typedef struct PLCrashSignalHandlerPorts {
    mach_msg_type_number_t  count;
    exception_mask_t        masks[EXC_TYPES_COUNT];
    exception_port_t        ports[EXC_TYPES_COUNT];
    exception_behavior_t    behaviors[EXC_TYPES_COUNT];
    thread_state_flavor_t   flavors[EXC_TYPES_COUNT];
} PLCrashSignalHandlerPorts;

/** @internal
 * Saved reference to the previous exception handler (process global) */
static PLCrashSignalHandlerPorts old_exception_handler;


/** @internal
 * Exception port (process global) */
static mach_port_t exceptionPort;


/** @internal
 * Shared signal handler. Only one may be allocated per process, for obvious reasons. */
static PLCrashSignalHandler *sharedHandler;


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
    /* Must only be called once per process */
    if (initialized) {
        [NSException raise: PLCrashReporterException format: @"Attempted to re-register per-process signal handler"];
        return NO;
    }
    
    /* Fetch the current exception handler */
    if (![self saveExceptionHandler: &old_exception_handler error: outError]) {
        return NO;
    }

    /* Register exception handler ports */
    if (![self registerHandlerPort: &exceptionPort error: outError]) {
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
    /* Run forever */
    mach_msg_server(plexc_server, 2048, exceptionPort, 0);
    abort();
}

/**
 * Forward the exception to the original saved handler. Brian Alliet's example code
 * was used as a reference here.
 */
static kern_return_t plcrash_exception_forward (mach_port_t thread,
                                                mach_port_t task,
                                           exception_type_t exception,
                                           exception_data_t code,
                                     mach_msg_type_number_t codeCnt)
{
    kern_return_t kr;
    mach_port_t dest = MACH_PORT_NULL;
    exception_behavior_t behavior;
    exception_behavior_t behavior_code64;
    thread_state_flavor_t flavor;

    thread_state_data_t thread_state;
    mach_msg_type_number_t thread_state_count = THREAD_STATE_MAX;

    /* Check for a matching registered exception handler */
    for (mach_msg_type_number_t i = 0; i < old_exception_handler.count; i++) {
        if (old_exception_handler.masks[i] & (1 << exception)) {
            dest = old_exception_handler.ports[i];
            behavior = old_exception_handler.behaviors[i];
            flavor = old_exception_handler.flavors[i];
            break;
        }
    }

    /* No match, nothing to do */
    if (dest == MACH_PORT_NULL)
        return KERN_SUCCESS;

    /* Map to 64-bit behavior */
    if (behavior & MACH_EXCEPTION_CODES) {
        behavior_code64 = behavior;
        behavior &= ~MACH_EXCEPTION_CODES;
    }

    /* If needed, fetch the thread state */
    if (behavior != EXCEPTION_DEFAULT) {
        NSLog(@"Requesting flavor %d", flavor);
        kr = thread_get_state(thread, flavor, thread_state, &thread_state_count);
        if (kr != KERN_SUCCESS) {
            ASYNC_DEBUG("Failed to get thread state while forwarding exception!");
            return KERN_FAILURE;
        }
    }

    /* Call the appropriate exception function */
    switch (behavior) {
        case EXCEPTION_DEFAULT:
            if (behavior_code64 != 0) {
                ASYNC_DEBUG("Call 64 with default");
                kr = mach_exception_raise(dest, thread, task, exception, (mach_exception_data_t) code, codeCnt);
            } else
                kr = exception_raise(dest, thread, task, exception, code, codeCnt);
            break;
        case EXCEPTION_STATE:
            if (behavior_code64 != 0)
                kr = mach_exception_raise_state(dest, exception, (mach_exception_data_t) code, codeCnt, &flavor, thread_state,
                                                thread_state_count, thread_state, &thread_state_count);
            else
                kr = exception_raise_state(dest, exception, code, codeCnt, &flavor, thread_state,
                                           thread_state_count, thread_state, &thread_state_count);
            break;
        case EXCEPTION_STATE_IDENTITY:
            if (behavior_code64 != 0)
                kr = mach_exception_raise_state_identity(dest, thread, task, exception, (mach_exception_data_t) code, codeCnt, &flavor, 
                                                         thread_state, thread_state_count, thread_state, &thread_state_count);
            else
                kr = exception_raise_state_identity(dest, thread, task, exception, code, codeCnt, &flavor, 
                                                    thread_state, thread_state_count, thread_state, &thread_state_count);
            break;
        default:
            ASYNC_DEBUG("Unhandled behavior type while forwarding exception!");
            kr = KERN_FAILURE;
            break;
    }

    return kr;
}

/**
 * Called by mach_msg_server on receipt of an exception
 */
kern_return_t plcrash_exception_raise (mach_port_t exception_port,
                                       mach_port_t thread,
                                       mach_port_t task,
                                  exception_type_t exception,
                                  exception_data_t code_vector,
                            mach_msg_type_number_t code_count)
{
    if (plcrash_exception_forward(thread, task, exception, code_vector, code_count) != KERN_SUCCESS) {
        ASYNC_DEBUG("Failed to forward exception message, forcing immediate termination via _exit(1)");
        _exit(1);
    }

    /* Terminate the process */
    return KERN_FAILURE;
}

kern_return_t plcrash_exception_raise_state (mach_port_t exception_port,
                                        exception_type_t exception,
                                  const exception_data_t code,
                                  mach_msg_type_number_t codeCnt,
                                                     int *flavor,
                                    const thread_state_t old_state,
                                  mach_msg_type_number_t old_stateCnt,
                                          thread_state_t new_state,
                                  mach_msg_type_number_t *new_stateCnt)
{
    // Unused & unsupported
    return KERN_FAILURE;
}

kern_return_t plcrash_exception_raise_state_identity (mach_port_t exception_port,
                                                      mach_port_t thread,
                                                      mach_port_t task,
                                                 exception_type_t exception,
                                                 exception_data_t code,
                                           mach_msg_type_number_t codeCnt,
                                                              int *flavor,
                                                   thread_state_t old_state,
                                           mach_msg_type_number_t old_stateCnt,
                                                   thread_state_t new_state,
                                           mach_msg_type_number_t *new_stateCnt)
{
    // Unused & unsupported
    return KERN_FAILURE;
}

/**
 * Save the current exception handler.
 */
- (BOOL) saveExceptionHandler: (PLCrashSignalHandlerPorts *) saved error: (NSError **) outError {
    assert(initialized == NO);

    kern_return_t kr;
    saved->count = EXC_TYPES_COUNT;
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
    
    assert(initialized == NO);

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
    
    assert(initialized == NO);
    
    /* Set up the exception handler thread. The thread should be extremely lightweight, as it only
     * exists to handle very unusual conditions.
     *
     * We use a very small stack size (64k or PTHREAD_STACK_MIN, whichever is larger) */
    if (pthread_attr_init(&attr) != 0) {
        [self populateError: outError errnoVal: errno description: @"Could not set pthread stack size"];
        return NO;
    }

#if 0
    if (pthread_attr_setstacksize(&attr, MAX(PTHREAD_STACK_MIN, 64 * 1024)) != 0) {
        [self populateError: outError errnoVal: errno description: @"Could not set pthread stack size"];
        goto cleanup;
    }
#endif
    
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