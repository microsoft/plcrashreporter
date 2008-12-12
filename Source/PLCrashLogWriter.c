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

#import <sys/time.h>

#import "PLCrashLogWriter.h"
#import "PLCrashAsync.h"
#import "PLCrashFrameWalker.h"

#if 0
static size_t writen (int fd, const void *data, size_t len) {
    size_t left;
    ssize_t written;
    const void *p;
    
    /* Loop until all bytes are written */
    p = data;
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
#endif

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
        return PLCRASH_EINTERNAL;
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
    /* Initialize the system information */
#if 0
    crashReport.system_info = &systemInfo;
    {
        struct timeval tv;

        /* Fetch the timestamp */
        if (gettimeofday(&tv, NULL) == 0) {
            systemInfo.timestamp = tv.tv_sec;
        } else {
            systemInfo.timestamp = 0;
        }

        /* Set the OS stats */
        systemInfo.os_version = writer->utsname.release;
        systemInfo.operating_system = PLCRASH_OS;
        systemInfo.machine_type = PLCRASH_MACHINE;
        systemInfo.machine_type = PLCRASH__MACHINE_TYPE__X86_32;
    }
#endif
    
    /* Threads */

    
    /* Crashed Thread */
    // Last is the register index, so increment to get the count
    const uint32_t regCount = PLFRAME_REG_LAST + 1;

    {
        plframe_cursor_t cursor;
        plframe_error_t frame_err;

        /* Create the crashed thread frame cursor */
        if ((frame_err = plframe_cursor_init(&cursor, crashctx)) != PLFRAME_ESUCCESS) {
            PLCF_DEBUG("Failed to initialize frame cursor for crashed thread: %s", plframe_strerror(frame_err));
            return PLCRASH_EINTERNAL;
        }

        /* Fetch the first frame */
        if ((frame_err = plframe_cursor_next(&cursor)) != PLFRAME_ESUCCESS) {
            PLCF_DEBUG("Could not fetch crashed thread frame: %s", plframe_strerror(frame_err));
            return PLCRASH_EINTERNAL;
        }
    
        /* Write out registers */
        for (int i = 0; i < regCount; i++) {
            
            /* Fetch the register */
            plframe_word_t regVal;
            if ((frame_err = plframe_get_reg(&cursor, i, &regVal)) != PLFRAME_ESUCCESS) {
                // Should never happen
                PLCF_DEBUG("Could not fetch register %i value: %s", i, strerror(frame_err));
                regVal = 0;
            }

#if 0
            /* Initialize the register struct */
            crashedRegisterValues[i] = init_value;
            crashedRegisterValues[i].value = regVal;
            // Cast to drop 'const', since we can't make a copy.
            // The string will never be modified.
            crashedRegisterValues[i].name = (char *) plframe_get_regname(i);
            
            crashedRegisters[i] = crashedRegisterValues + i;
#endif
        }
    }

    /* Binary Images */
    {
    }
    
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
        case PLCRASH_EINTERNAL:
            return "Internal error";
    }
    
    /* Should be unreachable */
    return "Unhandled error code";
}

/**
 * @} plcrash_log_writer
 */