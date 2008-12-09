/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008 Plausible Labs Cooperative, Inc.
 * All rights reserved.
 */

#import "PLCrashReporter.h"
#import "PLCrashSignalHandler.h"
#import "PLCrashFrameWalker.h"

#import <signal.h>
#import <ucontext.h>
#import <unistd.h>
#import <dlfcn.h>

// Crash log handler
static void dump_crash_log (int signal, siginfo_t *info, ucontext_t *uap);

/**
 * @internal
 * List of monitored fatal signals.
 */
static int fatal_signals[] = {
    SIGABRT,
    SIGBUS,
    SIGFPE,
    SIGILL,
    SIGSEGV
};


// A super simple debug 'function' that's async safe.
#define ASYNC_DEBUG(msg) {\
    const char output[] = "[PLCrashReporter] " msg "\n";\
    write(STDERR_FILENO, output, sizeof(output));\
}

/** @internal
 * number of signals in the fatal signals list */
static int n_fatal_signals = (sizeof(fatal_signals) / sizeof(fatal_signals[0]));


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
    
    /* Call the crash log dumper */
    dump_crash_log(signal, info, uapVoid);
    
    /* Re-raise the signal */
    raise(signal);
}


/** @internal
 * Shared signal handler. Only one may be allocated per process, for obvious reasons. */
static PLCrashSignalHandler *sharedHandler;


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
    sa.sa_flags = SA_SIGINFO;
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


/**
 * Execute the crash log handler.
 * @warning For testing purposes only.
 *
 * @param signal Signal to emulate.
 * @param code Signal code.
 * @param address Fault address.
 */
- (void) testHandlerWithSignal: (int) signal code: (int) code faultAddress: (void *) address {
    siginfo_t info;
    ucontext_t uap;
    
    /* Initialze a faux-siginfo instance */
    info.si_addr = address;
    info.si_errno = 0;
    info.si_pid = getpid();
    info.si_uid = getuid();
    info.si_code = code;
    info.si_status = signal;
    
    /* Get the current thread's ucontext */
    getcontext(&uap);
    
    /* Execute the crash log handler */
    dump_crash_log(signal, &info, &uap);
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

/**
 * Dump the process crash log.
 *
 * XXX Currently outputs to stderr using NSLog, must use the crash log writer
 * implementation (once available)
 */
static void dump_crash_log (int signal, siginfo_t *info, ucontext_t *uap) {
    /* Basic signal information */
    NSLog(@"Received signal %d (sig%s) code %d", signal, sys_signame[signal], info->si_code);
    NSLog(@"Signal fault address %p", info->si_addr);

    /* Get backtrace */
    {
        plframe_cursor_t cursor;
    
        /* Fetch the first frame */
        if (plframe_cursor_init(&cursor, uap) != PLFRAME_ESUCCESS) {
            ASYNC_DEBUG("Could not init cursor");
        }

        /* Fetch remaining frames */
        while (plframe_cursor_next(&cursor) == PLFRAME_ESUCCESS) {
            Dl_info info;
            plframe_word_t ip;
            
            if (plframe_get_reg(&cursor, PLFRAME_REG_IP, &ip) != PLFRAME_ESUCCESS) {
                NSLog(@"Failed to get frame instruction pointer");
                break;
            }

            if (dladdr((void *) ip, &info) == 0) {
                NSLog(@"Can't find symbol for %p",ip);
            } else {
                NSLog(@"Frame IP %p (%s) %s", ip, info.dli_sname, info.dli_fname);
            }
        }
    }
}

#if 0

static BOOL plbacktrace_init (ucontext_t *uap, backtrace_frame *frame) {
    // TODO: Sanity checks!

    frame->ip = (void *) uap->uc_mcontext->__ss.__eip;
    frame->fp = (void **) uap->uc_mcontext->__ss.__ebp;

    return YES;
}

static BOOL plbacktrace_next_frame (const backtrace_frame *currentFrame, backtrace_frame *nextFrame) {
    NSLog(@"Req for frame ip %p", currentFrame->ip);
    if (currentFrame->fp[0] == NULL || currentFrame->fp[1] == NULL)
        return NO;

    nextFrame->fp = currentFrame->fp[0];
    nextFrame->ip = currentFrame->fp[1];

    return YES;
}

#endif