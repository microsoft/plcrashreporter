/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008 Plausible Labs Cooperative, Inc.
 * All rights reserved.
 */

#import <sys/utsname.h>
#import <TargetConditionals.h>

/**
 * @defgroup plcrash_log_writer Crash Log Writer
 * @ingroup plcrash_log
 *
 * Implements an async-safe, zero allocation crash log writer C API, intended
 * to be called from the crash log signal handler.
 *
 * @{
 */

/* Platform/Architecture Defines */
#if TARGET_IPHONE_SIMULATOR
#  define PLCRASH_OS    PLCRASH__OPERATING_SYSTEM__IPHONE_SIMULATOR
#elif TARGET_OS_IPHONE
#  define PLCRASH_OS    PLCRASH__OPERATING_SYSTEM__IPHONE_OS
#elif TARGET_OS_MAC
#  define PLCRASH_OS    PLCRASH__OPERATING_SYSTEM__MAC_OS_X
#else
#error Unknown operating system
#endif

#ifdef __x86_64__
#  define PLCRASH_MACHINE PLCRASH__MACHINE_TYPE__X86_64

#elif defined(__i386__)
#  define PLCRASH_MACHINE PLCRASH__MACHINE_TYPE__X86_32

#elif defined(__arm__)
#  define PLCRASH_MACHINE PLCRASH__MACHINE_TYPE__ARM

#else
#  error Unknown machine architecture
#endif

/**
 * @internal
 *
 * Crash log writer context.
 */
typedef struct plcrash_writer {
    /** Output file descriptor */
    int fd;

    /** System uname() */
    struct utsname utsname;
} plcrash_writer_t;


/**
 * Error return codes.
 */
typedef enum  {
    /** Success */
    PLCRASH_ESUCCESS = 0,
    
    /** Unknown error (if found, is a bug) */
    PLCRASH_EUNKNOWN,
    
    /** The output file can not be opened or written to */
    PLCRASH_OUTPUT_ERR,
    
    /** No memory available (allocation failed) */
    PLCRASH_ENOMEM,
    
    /** Unsupported operation */
    PLCRASH_ENOTSUP,
    
    /** Invalid argument */
    PLCRASH_EINVAL,
    
    /** Internal error */
    PLCRASH_EINTERNAL,
} plcrash_error_t;


const char *plcrash_strerror (plcrash_error_t error);

plcrash_error_t plcrash_writer_init (plcrash_writer_t *writer, const char *path);
plcrash_error_t plcrash_writer_close (plcrash_writer_t *writer);
plcrash_error_t plcrash_writer_report (plcrash_writer_t *writer, siginfo_t *siginfo, ucontext_t *crashctx);

/**
 * @} plcrash_log_writer
 */