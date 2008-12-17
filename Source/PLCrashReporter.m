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

/** @internal
 * CrashReporter cache directory name. */
static NSString *PLCRASH_CACHE_DIR = @"com.plausiblelabs.crashreporter.data";

/** @internal
 * Crash Report file name. */
static NSString *PLCRASH_LIVE_CRASHREPORT = @"live_report.plcrash";

/** @internal
 * Directory containing crash reports queued for sending. */
static NSString *PLCRASH_QUEUED_DIR = @"queued_reports";

/** @internal
 * Maximum number of bytes that will be written to the crash report.
 * Used as a safety measure in case of implementation malfunction.
 *
 * We provide for a generous 64k here. Most crash reports
 * are approximately 7k.
 */
#define MAX_REPORT_BYTES (64 * 1024)

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
    int fd = open(sigctx->path, O_RDWR|O_CREAT|O_TRUNC, 0644);
    if (fd < 0) {
        PLCF_DEBUG("Could not open the crashlog output file: %s", strerror(errno));
        return;
    }

    /* Initialize the output context */
    plcrash_async_file_init(&file, fd, MAX_REPORT_BYTES);

    /* Write the crash log using the already-initialized writer */
    plcrash_log_writer_write(&sigctx->writer, &file, info, uap);
    plcrash_log_writer_close(&sigctx->writer);

    /* Finished */
    plcrash_async_file_flush(&file);
    plcrash_async_file_close(&file);
}


/**
 * @internal
 *
 * Uncaught exception handler. Sets the plcrash_log_writer_t's uncaught exception
 * field, and then triggers a SIGTRAP (synchronous exception) to cause a normal
 * exception dump.
 *
 * XXX: It is possible that another crash may occur between setting the uncaught 
 * exception field, and triggering the signal handler.
 */
static void uncaught_exception_handler (NSException *exception) {
    /* Set the uncaught exception */
    plcrash_log_writer_set_exception(&signal_handler_context.writer, exception);

    /* Trigger the crash handler */
    raise(SIGTRAP);
}


@interface PLCrashReporter (PrivateMethods)

- (BOOL) populateCrashReportDirectoryAndReturnError: (NSError **) outError;

- (NSString *) crashReportDirectory;
- (NSString *) queuedCrashReportDirectory;
- (NSString *) crashReportPath;

@end


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

    /* Fetch the application's bundle identifier for use in the crash report directory path. No occurances of '/' should ever
     * be in a bundle ID, but just to be safe, we escape them */
    NSString *bundleIdPath = [[[NSBundle mainBundle] bundleIdentifier] stringByReplacingOccurrencesOfString: @"/" withString: @"_"];

    /* Fetch the path to the crash report directory */
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSCachesDirectory, NSUserDomainMask, YES);
    NSString *cacheDir = [paths objectAtIndex: 0];
    _crashReportDirectory = [[[cacheDir stringByAppendingPathComponent: PLCRASH_CACHE_DIR] stringByAppendingPathComponent: bundleIdPath] retain];

    return self;
}

- (void) dealloc {
    [_crashReportDirectory release];
    [super dealloc];
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

    /* Create the directory tree */
    if (![self populateCrashReportDirectoryAndReturnError: outError])
        return NO;

    /* Set up the signal handler context */
    signal_handler_context.path = strdup([[self crashReportPath] UTF8String]); // NOTE: would leak if this were not a singleton struct
    plcrash_log_writer_init(&signal_handler_context.writer, @"", @""); // TODO set version and bundle id

    /* Enable the signal handler */
    if (![[PLCrashSignalHandler sharedHandler] registerHandlerWithCallback: &signal_handler_callback context: &signal_handler_context error: outError])
        return NO;

    /* Set the uncaught exception handler */
    NSSetUncaughtExceptionHandler(&uncaught_exception_handler);

    /* Success */
    _enabled = YES;
    return YES;
}


@end

/**
 * @internal
 *
 * Private Methods
 */
@implementation PLCrashReporter (PrivateMethods)

/**
 * Validate (and create if necessary) the crash reporter directory structure.
 */
- (BOOL) populateCrashReportDirectoryAndReturnError: (NSError **) outError {
    NSFileManager *fm = [NSFileManager defaultManager];
    
    /* Set up reasonable directory attributes */
    NSDictionary *attributes = [NSDictionary dictionaryWithObject: [NSNumber numberWithUnsignedLong: 0755] forKey: NSFilePosixPermissions];
    
    /* Create the top-level path */
    if (![fm fileExistsAtPath: [self crashReportDirectory]] &&
        ![fm createDirectoryAtPath: [self crashReportDirectory] withIntermediateDirectories: YES attributes: attributes error: outError])
    {
        return NO;
    }

    /* Create the queued crash report directory */
    if (![fm fileExistsAtPath: [self queuedCrashReportDirectory]] &&
        ![fm createDirectoryAtPath: [self queuedCrashReportDirectory] withIntermediateDirectories: YES attributes: attributes error: outError])
    {
        return NO;
    }

    return YES;
}

/**
 * Return the path to the crash reporter data directory.
 */
- (NSString *) crashReportDirectory {
    return _crashReportDirectory;
}


/**
 * Return the path to to-be-sent crash reports.
 */
- (NSString *) queuedCrashReportDirectory {
    return [[self crashReportDirectory] stringByAppendingPathComponent: PLCRASH_QUEUED_DIR];
}


/**
 * Return the path to live crash report (which may not yet, or ever, exist).
 */
- (NSString *) crashReportPath {
    return [[self crashReportDirectory] stringByAppendingPathComponent: PLCRASH_LIVE_CRASHREPORT];
}



@end
