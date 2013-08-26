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

#import "PLCrashReporter.h"
#import "CrashReporter.h"
#import "PLCrashSignalHandler.h"

#import "PLCrashAsync.h"
#import "PLCrashLogWriter.h"
#import "PLCrashFrameWalker.h"

#import "PLCrashReporterNSError.h"

#import <fcntl.h>
#import <dlfcn.h>
#import <mach-o/dyld.h>

#define NSDEBUG(msg, args...) {\
    NSLog(@"[PLCrashReporter] " msg, ## args); \
}

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
 * Fatal signals to be monitored.
 */
static int monitored_signals[] = {
    SIGABRT,
    SIGBUS,
    SIGFPE,
    SIGILL,
    SIGSEGV,
    SIGTRAP
};

/** @internal
 * number of signals in the fatal signals list */
static int monitored_signals_count = (sizeof(monitored_signals) / sizeof(monitored_signals[0]));

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
 * Shared dyld image list.
 */
static plcrash_async_image_list_t shared_image_list;


/**
 * @internal
 * 
 * Signal handler context (singleton)
 */
static plcrashreporter_handler_ctx_t signal_handler_context;


/**
 * @internal
 * 
 * The optional user-supplied callbacks invoked after the crash report has been written.
 */
static PLCrashReporterCallbacks crashCallbacks = {
    .version = 0,
    .context = NULL,
    .handleSignal = NULL
};

/**
 * @internal
 *
 * Signal handler callback.
 */
static bool signal_handler_callback (int signal, siginfo_t *info, ucontext_t *uap, void *context, PLCrashSignalHandlerCallback *next) {
    plcrashreporter_handler_ctx_t *sigctx = context;
    plcrash_async_thread_state_t thread_state;
    plcrash_async_file_t file;
    
    /* Remove all signal handlers -- if the crash reporting code fails, the default terminate
     * action will occur.
     *
     * NOTE: SA_RESETHAND breaks SA_SIGINFO on ARM, so we reset the handlers manually.
     * http://openradar.appspot.com/11839803
     *
     * TODO: When forwarding signals (eg, to Mono's runtime), resetting the signal handlers
     * could result in incorrect runtime behavior; we should revisit resetting the
     * signal handlers once we address double-fault handling.
     */
    for (int i = 0; i < monitored_signals_count; i++) {
        struct sigaction sa;
        
        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = SIG_DFL;
        sigemptyset(&sa.sa_mask);
        
        sigaction(monitored_signals[i], &sa, NULL);
    }

    /* Extract the thread state */
    plcrash_async_thread_state_mcontext_init(&thread_state, uap->uc_mcontext);

    /* Open the output file */
    int fd = open(sigctx->path, O_RDWR|O_CREAT|O_TRUNC, 0644);
    if (fd < 0) {
        PLCF_DEBUG("Could not open the crashlog output file: %s", strerror(errno));
        return false;
    }

    /* Initialize the output context */
    plcrash_async_file_init(&file, fd, MAX_REPORT_BYTES);

    /* Write the crash log using the already-initialized writer */
    plcrash_log_writer_write(&sigctx->writer, mach_thread_self(), &shared_image_list, &file, info, &thread_state);
    plcrash_log_writer_close(&sigctx->writer);

    /* Finished */
    plcrash_async_file_flush(&file);
    plcrash_async_file_close(&file);

    /* Call any post-crash callback */
    if (crashCallbacks.handleSignal != NULL)
        crashCallbacks.handleSignal(info, uap, crashCallbacks.context);
    
    return false;
}

/**
 * @internal
 * dyld image add notification callback.
 */
static void image_add_callback (const struct mach_header *mh, intptr_t vmaddr_slide) {
    Dl_info info;
    
    /* Look up the image info */
    if (dladdr(mh, &info) == 0) {
        NSLog(@"%s: dladdr(%p, ...) failed", __FUNCTION__, mh);
        return;
    }

    /* Register the image */
    plcrash_nasync_image_list_append(&shared_image_list, (pl_vm_address_t) mh, info.dli_fname);
}

/**
 * @internal
 * dyld image remove notification callback.
 */
static void image_remove_callback (const struct mach_header *mh, intptr_t vmaddr_slide) {
    plcrash_nasync_image_list_remove(&shared_image_list, (uintptr_t) mh);
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

    /* Synchronously trigger the crash handler */
    abort();
}


@interface PLCrashReporter (PrivateMethods)

- (id) initWithBundle: (NSBundle *) bundle configuration: (PLCrashReporterConfig *) configuration;
- (id) initWithApplicationIdentifier: (NSString *) applicationIdentifier appVersion: (NSString *) applicationVersion configuration: (PLCrashReporterConfig *) configuration;

- (BOOL) populateCrashReportDirectoryAndReturnError: (NSError **) outError;
- (NSString *) crashReportDirectory;
- (NSString *) queuedCrashReportDirectory;
- (NSString *) crashReportPath;

@end


/**
 * Crash Reporter.
 *
 * A PLCrashReporter instance manages process-wide handling of crashes.
 */
@implementation PLCrashReporter

+ (void) initialize {
    if (![[self class] isEqual: [PLCrashReporter class]])
        return;

    /* Enable dyld image monitoring */
    plcrash_nasync_image_list_init(&shared_image_list, mach_task_self());
    _dyld_register_func_for_add_image(image_add_callback);
    _dyld_register_func_for_remove_image(image_remove_callback);
}


/* (Deprecated) Crash reporter singleton. */
static PLCrashReporter *sharedReporter = nil;

/**
 * Return the default crash reporter instance. The returned instance will be configured
 * appropriately for release deployment.
 *
 * @deprecated As of PLCrashReporter 1.2, the default reporter instance has been deprecated, and API
 * clients should initialize a crash reporter instance directly.
 */
+ (PLCrashReporter *) sharedReporter {
    /* Once we drop 10.5 support, this may be converted to dispatch_once() */
    static OSSpinLock onceLock = OS_SPINLOCK_INIT;
    OSSpinLockLock(&onceLock); {
        if (sharedReporter == nil)
            sharedReporter = [[PLCrashReporter alloc] initWithBundle: [NSBundle mainBundle] configuration: [PLCrashReporterConfig defaultConfiguration]];
    } OSSpinLockUnlock(&onceLock);

    return sharedReporter;
}

/**
 * Initialize a new PLCrashReporter instance with the given configuration.
 *
 * @param configuration The configuration to be used by this reporter instance.
 */
- (instancetype) initWithConfiguration: (PLCrashReporterConfig *) configuration {
    return [self initWithBundle: [NSBundle mainBundle] configuration: configuration];
}


/**
 * Returns YES if the application has previously crashed and
 * an pending crash report is available.
 */
- (BOOL) hasPendingCrashReport {
    /* Check for a live crash report file */
    return [[NSFileManager defaultManager] fileExistsAtPath: [self crashReportPath]];
}


/**
 * If an application has a pending crash report, this method returns the crash
 * report data.
 *
 * You may use this to submit the report to your own HTTP server, over e-mail, or even parse and
 * introspect the report locally using the PLCrashReport API.
 *
 * @return Returns nil if the crash report data could not be loaded.
 */
- (NSData *) loadPendingCrashReportData {
    return [self loadPendingCrashReportDataAndReturnError: NULL];
}


/**
 * If an application has a pending crash report, this method returns the crash
 * report data.
 *
 * You may use this to submit the report to your own HTTP server, over e-mail, or even parse and
 * introspect the report locally using the PLCrashReport API.
 
 * @param outError A pointer to an NSError object variable. If an error occurs, this pointer
 * will contain an error object indicating why the pending crash report could not be
 * loaded. If no error occurs, this parameter will be left unmodified. You may specify
 * nil for this parameter, and no error information will be provided.
 *
 * @return Returns nil if the crash report data could not be loaded.
 */
- (NSData *) loadPendingCrashReportDataAndReturnError: (NSError **) outError {
    /* Load the (memory mapped) data */
    return [NSData dataWithContentsOfFile: [self crashReportPath] options: NSMappedRead error: outError];
}


/**
 * Purge a pending crash report.
 *
 * @return Returns YES on success, or NO on error.
 */
- (BOOL) purgePendingCrashReport {
    return [self purgePendingCrashReportAndReturnError: NULL];
}


/**
 * Purge a pending crash report.
 *
 * @return Returns YES on success, or NO on error.
 */
- (BOOL) purgePendingCrashReportAndReturnError: (NSError **) outError {
    return [[NSFileManager defaultManager] removeItemAtPath: [self crashReportPath] error: outError];
}


/**
 * Enable the crash reporter. Once called, all application crashes will
 * result in a crash report being written prior to application exit.
 *
 * @return Returns YES on success, or NO if the crash reporter could
 * not be enabled.
 *
 * @par Registering Multiple Reporters
 *
 * Only one PLCrashReporter instance may be enabled in a process; attempting to enable an additional instance
 * will return NO, and the reporter will not be enabled. This restriction may be removed in a future release.
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
 * will contain an error in the PLCrashReporterErrorDomain indicating why the Crash Reporter
 * could not be enabled. If no error occurs, this parameter will be left unmodified. You may
 * specify nil for this parameter, and no error information will be provided.
 *
 * @return Returns YES on success, or NO if the crash reporter could
 * not be enabled.
 *
 * @par Registering Multiple Reporters
 *
 * Only one PLCrashReporter instance may be enabled in a process; attempting to enable an additional instance
 * will return NO and a PLCrashReporterErrorResourceBusy error, and the reporter will not be enabled.
 * This restriction may be removed in a future release.
 */
- (BOOL) enableCrashReporterAndReturnError: (NSError **) outError {
    /* Prevent enabling more than one crash reporter, process wide. We can not support multiple chained reporters
     * due to the use of NSUncaughtExceptionHandler (it doesn't support chaining or assocation of context with the callbacks), as
     * well as our legacy approach of deregistering any signal handlers upon the first signal. Once PLCrashUncaughtExceptionHandler is
     * implemented, and we support double-fault handling without resetting the signal handlers, we can support chaining of multiple
     * crash reporters. */
    {
        static BOOL enforceOne = NO;
        pthread_mutex_t enforceOneLock = PTHREAD_MUTEX_INITIALIZER;
        pthread_mutex_lock(&enforceOneLock); {
            if (enforceOne) {
                pthread_mutex_unlock(&enforceOneLock);
                plcrash_populate_error(outError, PLCrashReporterErrorResourceBusy, @"A PLCrashReporter instance has already been enabled", nil);
                return NO;
            }
            enforceOne = YES;
        } pthread_mutex_unlock(&enforceOneLock);
    }

    /* Check for programmer error */
    if (_enabled)
        [NSException raise: PLCrashReporterException format: @"The crash reporter has alread been enabled"];

    /* Create the directory tree */
    if (![self populateCrashReportDirectoryAndReturnError: outError])
        return NO;

    /* Set up the signal handler context */
    signal_handler_context.path = strdup([[self crashReportPath] UTF8String]); // NOTE: would leak if this were not a singleton struct
    assert(_applicationIdentifier != nil);
    assert(_applicationVersion != nil);
    plcrash_log_writer_init(&signal_handler_context.writer, _applicationIdentifier, _applicationVersion, false);

    /* Enable the signal handler */
    for (size_t i = 0; i < monitored_signals_count; i++) {
        if (![[PLCrashSignalHandler sharedHandler] registerHandlerForSignal: monitored_signals[i] callback: &signal_handler_callback context: &signal_handler_context error: outError])
            return NO;
    }

    /* Set the uncaught exception handler */
    NSSetUncaughtExceptionHandler(&uncaught_exception_handler);

    /* Success */
    _enabled = YES;
    return YES;
}

/**
 * Generate a live crash report for a given @a thread, without triggering an actual crash condition.
 * This may be used to log current process state without actually crashing. The crash report data will be
 * returned on success.
 *
 * @param thread The thread which will be marked as the failing thread in the generated report.
 *
 * @return Returns nil if the crash report data could not be generated.
 *
 * @sa PLCrashReporter::generateLiveReportWithMachThread:error:
 */
- (NSData *) generateLiveReportWithThread: (thread_t) thread {
    return [self generateLiveReportWithThread: thread error: NULL];
}


/* State and callback used by -generateLiveReportWithThread */
struct plcr_live_report_context {
    plcrash_log_writer_t *writer;
    plcrash_async_file_t *file;
    siginfo_t *info;
};
static plcrash_error_t plcr_live_report_callback (plcrash_async_thread_state_t *state, void *ctx) {
    struct plcr_live_report_context *plcr_ctx = ctx;
    return plcrash_log_writer_write(plcr_ctx->writer, mach_thread_self(), &shared_image_list, plcr_ctx->file, plcr_ctx->info, state);
}


/**
 * Generate a live crash report for a given @a thread, without triggering an actual crash condition.
 * This may be used to log current process state without actually crashing. The crash report data will be
 * returned on success.
 *
 * @param thread The thread which will be marked as the failing thread in the generated report.
 * @param outError A pointer to an NSError object variable. If an error occurs, this pointer
 * will contain an error object indicating why the crash report could not be generated or loaded. If no
 * error occurs, this parameter will be left unmodified. You may specify nil for this parameter, and no
 * error information will be provided.
 *
 * @return Returns nil if the crash report data could not be loaded.
 *
 * @todo Implement in-memory, rather than requiring writing of the report to disk.
 */
- (NSData *) generateLiveReportWithThread: (thread_t) thread error: (NSError **) outError {
    plcrash_log_writer_t writer;
    plcrash_async_file_t file;
    plcrash_error_t err;

    /* Open the output file */
    NSString *templateStr = [NSTemporaryDirectory() stringByAppendingPathComponent: @"live_crash_report.XXXXXX"];
    char *path = strdup([templateStr fileSystemRepresentation]);
    
    int fd = mkstemp(path);
    if (fd < 0) {
        plcrash_populate_posix_error(outError, errno, NSLocalizedString(@"Failed to create temporary path", @"Error opening temporary output path"));
        free(path);

        return nil;
    }

    /* Initialize the output context */
    plcrash_log_writer_init(&writer, _applicationIdentifier, _applicationVersion, true);
    plcrash_async_file_init(&file, fd, MAX_REPORT_BYTES);
    
    /* Mock up a SIGTRAP-based siginfo_t */
    siginfo_t info;
    memset(&info, 0, sizeof(info));
    info.si_signo = SIGTRAP;
    info.si_code = TRAP_TRACE;
    info.si_addr = __builtin_return_address(0);
    
    /* Write the crash log using the already-initialized writer */
    if (thread == mach_thread_self()) {
        struct plcr_live_report_context ctx = {
            .writer = &writer,
            .file = &file,
            .info = &info
        };
        err = plcrash_async_thread_state_current(plcr_live_report_callback, &ctx);
    } else {
        err = plcrash_log_writer_write(&writer, thread, &shared_image_list, &file, &info, NULL);
    }
    plcrash_log_writer_close(&writer);

    /* Flush the data */
    plcrash_async_file_flush(&file);
    plcrash_async_file_close(&file);

    /* Check for write failure */
    NSData *data;
    if (err != PLCRASH_ESUCCESS) {
        NSLog(@"Write failed with error %s", plcrash_async_strerror(err));
        plcrash_populate_error(outError, PLCrashReporterErrorUnknown, @"Failed to write the crash report to disk", nil);
        data = nil;
        goto cleanup;
    }

    data = [NSData dataWithContentsOfFile: [NSString stringWithUTF8String: path]];
    if (data == nil) {
        /* This should only happen if our data is deleted out from under us */
        plcrash_populate_error(outError, PLCrashReporterErrorUnknown, NSLocalizedString(@"Unable to open live crash report for reading", nil), nil);
        goto cleanup;
    }

cleanup:
    /* Finished -- clean up. */
    plcrash_log_writer_free(&writer);

    if (unlink(path) != 0) {
        /* This shouldn't fail, but if it does, there's no use in returning nil */
        NSLog(@"Failure occured deleting live crash report: %s", strerror(errno));
    }

    free(path);
    return data;
}


/**
 * Generate a live crash report, without triggering an actual crash condition. This may be used to log
 * current process state without actually crashing. The crash report data will be returned on
 * success.
 * 
 * @return Returns nil if the crash report data could not be loaded.
 */
- (NSData *) generateLiveReport {
    return [self generateLiveReportAndReturnError: NULL];
}


/**
 * Generate a live crash report for the current thread, without triggering an actual crash condition.
 * This may be used to log current process state without actually crashing. The crash report data will be
 * returned on success.
 *
 * @param outError A pointer to an NSError object variable. If an error occurs, this pointer
 * will contain an error object indicating why the pending crash report could not be
 * generated or loaded. If no error occurs, this parameter will be left unmodified. You may specify
 * nil for this parameter, and no error information will be provided.
 * 
 * @return Returns nil if the crash report data could not be loaded.
 */
- (NSData *) generateLiveReportAndReturnError: (NSError **) outError {
    return [self generateLiveReportWithThread: mach_thread_self() error: outError];
}


/**
 * Set the callbacks that will be executed by the receiver after a crash has occured and been recorded by PLCrashReporter.
 *
 * @param callbacks A pointer to an initialized PLCrashReporterCallbacks structure.
 *
 * @note This method must be called prior to PLCrashReporter::enableCrashReporter or
 * PLCrashReporter::enableCrashReporterAndReturnError:
 *
 * @sa @ref async_safety
 */
- (void) setCrashCallbacks: (PLCrashReporterCallbacks *) callbacks {
    /* Check for programmer error; this should not be called after the signal handler is enabled as to ensure that
     * the signal handler can never fire with a partially initialized callback structure. */
    if (_enabled)
        [NSException raise: PLCrashReporterException format: @"The crash reporter has alread been enabled"];

    assert(callbacks->version == 0);

    /* Re-initialize our internal callback structure */
    crashCallbacks.version = 0;

    /* Re-configure the saved callbacks */
    crashCallbacks.context = callbacks->context;
    crashCallbacks.handleSignal = callbacks->handleSignal;
}


@end

/**
 * @internal
 *
 * Private Methods
 */
@implementation PLCrashReporter (PrivateMethods)

/**
 * @internal
 *
 * This is the designated initializer, but it is not intended
 * to be called externally.
 *
 * @param applicationIdentifier The application identifier to be included in crash reports.
 * @param applicationVersion The application version number to be included in crash reports.
 * @param configuration The PLCrashReporter configuration.
 *
 * @todo The appId and version values should be fetched from the PLCrashReporterConfig, once the API
 * has been extended to allow supplying these values.
 */
- (id) initWithApplicationIdentifier: (NSString *) applicationIdentifier appVersion: (NSString *) applicationVersion configuration: (PLCrashReporterConfig *) configuration {
    /* Initialize our superclass */
    if ((self = [super init]) == nil)
        return nil;

    /* Save the configuration */
    _config = [configuration retain];
    _applicationIdentifier = [applicationIdentifier retain];
    _applicationVersion = [applicationVersion retain];
    
    /* No occurances of '/' should ever be in a bundle ID, but just to be safe, we escape them */
    NSString *appIdPath = [applicationIdentifier stringByReplacingOccurrencesOfString: @"/" withString: @"_"];
    
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSCachesDirectory, NSUserDomainMask, YES);
    NSString *cacheDir = [paths objectAtIndex: 0];
    _crashReportDirectory = [[[cacheDir stringByAppendingPathComponent: PLCRASH_CACHE_DIR] stringByAppendingPathComponent: appIdPath] retain];
    
    return self;
}


/**
 * @internal
 * 
 * Derive the bundle identifier and version from @a bundle.
 *
 * @param bundle The application's main bundle.
 * @param configuration The PLCrashReporter configuration to use for this instance.
 */
- (id) initWithBundle: (NSBundle *) bundle configuration: (PLCrashReporterConfig *) configuration {
    NSString *bundleIdentifier = [bundle bundleIdentifier];
    NSString *bundleVersion = [[bundle infoDictionary] objectForKey: (NSString *) kCFBundleVersionKey];
    
    /* Verify that the identifier is available */
    if (bundleIdentifier == nil) {
        const char *progname = getprogname();
        if (progname == NULL) {
            [NSException raise: PLCrashReporterException format: @"Can not determine process identifier or process name"];
            [self release];
            return nil;
        }

        NSDEBUG(@"Warning -- bundle identifier, using process name %s", progname);
        bundleIdentifier = [NSString stringWithUTF8String: progname];
    }

    /* Verify that the version is available */
    if (bundleVersion == nil) {
        NSDEBUG(@"Warning -- bundle version unavailable");
        bundleVersion = @"";
    }
    
    return [self initWithApplicationIdentifier: bundleIdentifier appVersion: bundleVersion configuration: configuration];
}


- (void) dealloc {
    [_crashReportDirectory release];
    [_config release];
    [_applicationIdentifier release];
    [_applicationVersion release];

    [super dealloc];
}


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
