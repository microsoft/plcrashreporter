/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008 Plausible Labs Cooperative, Inc.
 * All rights reserved.
 */

#import <sys/utsname.h>

/**
 * @defgroup plcras_log_writer Crash Log Writer
 * @ingroup plcrash_log
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
    PLCRASH_INTERNAL,
} plcrash_error_t;


const char *plcrash_strerror (plcrash_error_t error);

plcrash_error_t plcrash_writer_init (plcrash_writer_t *writer, const char *path);
plcrash_error_t plcrash_writer_close (plcrash_writer_t *writer);

/**
 * @} plcrash_log
 */