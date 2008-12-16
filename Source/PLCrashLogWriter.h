/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008 Plausible Labs Cooperative, Inc.
 * All rights reserved.
 */

#import <sys/utsname.h>
#import <TargetConditionals.h>

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
typedef struct plcrash_writer {
    /** System uname() */
    struct utsname utsname;
} plcrash_writer_t;


plcrash_error_t plcrash_writer_init (plcrash_writer_t *writer);
plcrash_error_t plcrash_writer_write (plcrash_writer_t *writer, plasync_file_t *file, siginfo_t *siginfo, ucontext_t *crashctx);
plcrash_error_t plcrash_writer_close (plcrash_writer_t *writer);

/**
 * @} plcrash_log_writer
 */