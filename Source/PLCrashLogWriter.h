/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008 Plausible Labs Cooperative, Inc.
 * All rights reserved.
 */

#import <sys/utsname.h>
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

/** Crash file magic identifier */
#define PLCRASH_LOG_FILE_MAGIC "plcrash"

/** Crash format version byte identifier. Will not change outside of the introduction of
 * an entirely new crash log format. */
#define PLCRASH_LOG_FILE_VERSION 1

/** CrashReport machine type enums */
enum {
    PLCRASH_MACHINE_TYPE_X86_32 = 0,
    PLCRASH_MACHINE_TYPE_X86_64 = 1,
    PLCRASH_MACHINE_TYPE_ARM = 2
};

/** CrashReport operating system enums */
enum {
    PLCRASH_OPERATING_SYSTEM_MAC_OS_X = 0,
    PLCRASH_OPERATING_SYSTEM_IPHONE_OS = 1,
    PLCRASH_OPERATING_SYSTEM_IPHONE_SIMULATOR = 2
};


/* Platform/Architecture Defines */
#if TARGET_IPHONE_SIMULATOR
#  define PLCRASH_OS    PLCRASH_OPERATING_SYSTEM_IPHONE_SIMULATOR
#elif TARGET_OS_IPHONE
#  define PLCRASH_OS    PLCRASH_OPERATING_SYSTEM_IPHONE_OS
#elif TARGET_OS_MAC
#  define PLCRASH_OS    PLCRASH_OPERATING_SYSTEM_MAC_OS_X
#else
#error Unknown operating system
#endif

#ifdef __x86_64__
#  define PLCRASH_MACHINE PLCRASH_MACHINE_TYPE_X86_64

#elif defined(__i386__)
#  define PLCRASH_MACHINE PLCRASH_MACHINE_TYPE_X86_32

#elif defined(__arm__)
#  define PLCRASH_MACHINE PLCRASH_MACHINE_TYPE_ARM

#else
#  error Unknown machine architecture
#endif

/**
 * @internal
 *
 * Crash log writer context.
 */
typedef struct plcrash_log_writer {
    /** System uname() */
    struct utsname utsname;

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
