/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008 Plausible Labs Cooperative, Inc.
 * All rights reserved.
 */

#import "PLCrashReporter.h"
#import "CrashReporter.h"
#import "PLCrashSignalHandler.h"

#import "PLCrashAsync.h"
#import "PLCrashLogWriter.h"

#import <fcntl.h>

/**
 * @internal
 *
 * Crash reporter singleton. Must default to NULL,
 * or the -[PLCrashReporter init] method will fail when attempting
 * to detect if another crash reporter exists.
 */
static PLCrashReporter *sharedReporter = NULL;


/**
 * @internal
 * Signal handler context
 */
typedef struct signal_handler_ctx {
    /** PLCrashLogWriter instance */
    plcrash_log_writer_t writer;

    /** Path to the output file */
    const char *path;
} plcrashreporter_handler_ctx_t;


/**
 * @internal
 * 
 * Signal handler context (singleton)
 */
static plcrashreporter_handler_ctx_t signal_handler_context;

/**
 * @internal
 *
 * Signal handler callback.
 */
static void signal_handler_callback (int signal, siginfo_t *info, ucontext_t *uap, void *context) {
    plcrashreporter_handler_ctx_t *sigctx = context;
    plcrash_async_file_t file;

    /* Open the output file */
    int fd = open(sigctx->path, O_RDWR|O_CREAT|O_EXCL, 0644);
    if (fd < 0) {
        PLCF_DEBUG("Could not open the crashlog output file: %s", strerror(errno));
        return;
    }

    /* Initialize the output context */
    plcrash_async_file_init(&file, fd);

    /* Write the crash log using the already-initialized writer */
    plcrash_log_writer_write(&sigctx->writer, &file, info, uap);
    plcrash_log_writer_close(&sigctx->writer);

    /* Finished */
    plcrash_async_file_flush(&file);
    plcrash_async_file_close(&file);
}


/**
 * Shared application crash reporter.
 */
@implementation PLCrashReporter

/* Create the shared crash reporter singleton. */
+ (void) initialize {
    sharedReporter = NULL;
    sharedReporter = [[PLCrashReporter alloc] init];
}

/**
 * Return the application's crash reporter instance.
 */
+ (PLCrashReporter *) sharedReporter {
    return sharedReporter;
}


/**
 * @internal
 *
 * This is the designated class initializer, but it is not intended
 * to be called externally. If 
 */
- (id) init {
    /* Only allow one instance to be created, no matter what */
    if (sharedReporter != NULL) {
        [self release];
        return sharedReporter;
    }

    /* Initialize our superclass */
    if ((self = [super init]) == nil)
        return nil;

    return self;
}


/**
 * Enable the crash reporter. Once called, all application crashes will
 * result in a crash report being written prior to application exit.
 *
 * @return Returns YES on success, or NO if the crash reporter could
 * not be enabled.
 */
- (BOOL) enableCrashReporter {
    return [self enableCrashReporterAndReturnError: nil];
}



/**
 * Enable the crash reporter. Once called, all application crashes will
 * result in a crash report being written prior to application exit.
 *
 * This method must only be invoked once. Further invocations will throw
 * a PLCrashReporterException.
 *
 * @param outError A pointer to an NSError object variable. If an error occurs, this pointer
 * will contain an error object indicating why the Crash Reporter could not be enabled.
 * If no error occurs, this parameter will be left unmodified. You may specify nil for this
 * parameter, and no error information will be provided.
 *
 * @return Returns YES on success, or NO if the crash reporter could
 * not be enabled.
 */
- (BOOL) enableCrashReporterAndReturnError: (NSError **) outError {
    /* Check for programmer error */
    if (_enabled)
        [NSException raise: PLCrashReporterException format: @"The crash reporter has alread been enabled"];

    /* Set up the signal handler context */
    signal_handler_context.path = NULL; // TODO
    plcrash_log_writer_init(&signal_handler_context.writer);

    /* Enable the signal handler */
    if ([[PLCrashSignalHandler sharedHandler] registerHandlerWithCallback: &signal_handler_callback context: NULL error: outError])
        return NO;

    /* Success */
    _enabled = YES;
    return YES;
}


@end
