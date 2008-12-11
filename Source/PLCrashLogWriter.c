/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008 Plausible Labs Cooperative, Inc.
 * All rights reserved.
 */

#import <stdlib.h>
#import <fcntl.h>
#import <errno.h>
#import <string.h>
#import <stdbool.h>

#import "PLCrashLogWriter.h"
#import "PLCrashAsync.h"

#import "crash_report.pb-c.h"


/**
 * @internal
 * @ingroup plcrash_log_writer
 *
 * @{
 */
static ssize_t writen (int fd, const uint8_t *buf, size_t len) {
    const uint8_t *p;
    size_t left;
    ssize_t written;

    /* Loop until all bytes are written */
    p = buf;
    left = len;
    while (left > 0) {
        if ((written = write(fd, p, left)) <= 0) {
            if (errno == EINTR) {
                // Try again
                written = 0;
            } else {
                PLCF_DEBUG("Error occured writing to crash log: %s", strerror(errno));
                return -1;
            }
        }
            
        left -= written;
        p += written;
    }

    return written;
}

/**
 * Initialize a new crash log writer instance. This fetches all necessary environment
 * information.
 *
 * @param writer Writer instance to be initialized.
 * @param path Crash log output path.
 *
 * @warning This function is not guaranteed to be async-safe, and must be
 * called prior to entering the crash handler.
 */
plcrash_error_t plcrash_writer_init (plcrash_writer_t *writer, const char *path) {
    /* Try opening the output file */
    writer->fd = open(path, O_WRONLY|O_TRUNC|O_CREAT);
    if (writer->fd < 0) {
        PLCF_DEBUG("Open of '%s' failed: %s", path, strerror(errno));
        return PLCRASH_OUTPUT_ERR;
    }

    /* Fetch the OS information */
    if (uname(&writer->utsname) != 0) {
        // Should not happen
        PLCF_DEBUG("uname() failed: %s", strerror(errno));
        return PLCRASH_INTERNAL;
    }

    return PLCRASH_ESUCCESS;
}


/**
 * Write the crash report. All other running threads are suspended while the crash report is generated.
 *
 * @warning This method must only be called from the thread that has triggered the crash. This must correspond
 * to the provided crashctx. Failure to adhere to this requirement will result in an invalid stack trace
 * and thread dump.
 */
plcrash_error_t plcrash_writer_report (plcrash_writer_t *writer, siginfo_t *siginfo, ucontext_t *crashctx) {
    uint8_t buffer[] = { 0, 1 };

    writen(writer->fd, buffer, sizeof(buffer));

    return PLCRASH_ESUCCESS;
}

/**
 * Close the plcrash_writer_t output.
 *
 * @param writer Writer instance to be closed.
 */
plcrash_error_t plcrash_writer_close (plcrash_writer_t *writer) {
    /* Try closing the output file */
    if (close(writer->fd) != 0) {
        PLCF_DEBUG("Closing output file failed: %s", strerror(errno));
        return PLCRASH_OUTPUT_ERR;
    }

    return PLCRASH_ESUCCESS;
}

/**
 * Return an error description for the given plcrash_error_t.
 */
const char *plcrash_strerror (plcrash_error_t error) {
    switch (error) {
        case PLCRASH_ESUCCESS:
            return "No error";
        case PLCRASH_EUNKNOWN:
            return "Unknown error";
        case PLCRASH_OUTPUT_ERR:
            return "Output file can not be opened (or written to)";
        case PLCRASH_ENOMEM:
            return "No memory available";
        case PLCRASH_ENOTSUP:
            return "Operation not supported";
        case PLCRASH_EINVAL:
            return "Invalid argument";
        case PLCRASH_INTERNAL:
            return "Internal error";
    }
    
    /* Should be unreachable */
    return "Unhandled error code";
}

/**
 * @} plcrash_log_writer
 */