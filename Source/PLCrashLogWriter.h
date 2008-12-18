/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008 Plausible Labs Cooperative, Inc.
 * All rights reserved.
 */

#import <TargetConditionals.h>
#import <Foundation/Foundation.h>

#import "PLCrashAsync.h"

/**
 * @defgroup plcrash_log_writer Crash Log Writer
 * @ingroup plcrash_internal
 *
 * Implements an async-safe, zero allocation crash log writer C API, intended
 * to be called from the crash log signal handler.
 *
 * @{
 */

/**
 * @internal
 *
 * Crash log writer context.
 */
typedef struct plcrash_log_writer {
    struct {
        char *version;
    } system_info;

    /** Application data */
    struct {
        /** Application identifier */
        char *app_identifier;

        /** Application version */
        char *app_version;
    } application_info;

    /** Uncaught exception (if any) */
    struct {
        /** Flag specifying wether an uncaught exception is available. */
        bool has_exception;

        /** Exception name (may be null) */
        char *name;

        /** Exception reason (may be null) */
        char *reason;

        /** Actual exception. Can not be accessed async safe! */
        NSException *exception;
    } uncaught_exception;
} plcrash_log_writer_t;


plcrash_error_t plcrash_log_writer_init (plcrash_log_writer_t *writer, NSString *app_identifier, NSString *app_version);
void plcrash_log_writer_set_exception (plcrash_log_writer_t *writer, NSException *exception);
plcrash_error_t plcrash_log_writer_write (plcrash_log_writer_t *writer, plcrash_async_file_t *file, siginfo_t *siginfo, ucontext_t *crashctx);
plcrash_error_t plcrash_log_writer_close (plcrash_log_writer_t *writer);
void plcrash_log_writer_free (plcrash_log_writer_t *writer);

/**
 * @} plcrash_log_writer
 */
