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

#import "PLCrashLog.h"
#import "PLCrashLogWriter.h"
#import "PLCrashAsyncDebug.h"

#import "crash_report.pb-c.h"

// Simple file descriptor output buffer
typedef struct pl_protofd_buffer {
    ProtobufCBuffer base;
    int fd;
    bool had_error;
} pl_protofd_buffer_t;

static void fd_buffer_append (ProtobufCBuffer *buffer, size_t len, const uint8_t *data) {
    pl_protofd_buffer_t *fd_buf = (pl_protofd_buffer_t *) buffer;
    const uint8_t *p;
    size_t left;
    ssize_t written;
    
    /* If an error has occured, don't try to write */
    if (fd_buf->had_error)
        return;

    /* Loop until all bytes are written */
    p = data;
    left = len;
    while (left > 0) {
        if ((written = write(fd_buf->fd, p, left)) <= 0) {
            if (errno == EINTR) {
                // Try again
                written = 0;
            } else {
                PLCF_DEBUG("Error occured writing to crash log: %s", strerror(errno));
                fd_buf->had_error = true;
                return;
            }

            left -= written;
            p += written;
        }
    }
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

struct Message {
    Plausible__Crashreport__CrashReport__SystemInfo system_info;
    Plausible__Crashreport__CrashReport__Thread threads;
    Plausible__Crashreport__CrashReport__ThreadState crashed_thread_state;
    Plausible__Crashreport__CrashReport__BinaryImage images;
};

/**
 * Write the crash report. All other running threads are suspended while the
 * crash report is generated.
 *
 * @warning This method must only be called from the thread that has triggered the crash. This must correspond
 * to the provided crashctx. Failure to adhere to this requirement will result in an invalid stack trace
 * and thread dump.
 */
plcrash_error_t plcrash_writer_report (plcrash_writer_t *writer, siginfo_t *siginfo, ucontext_t *crashctx) {
    /* Initialize the output buffer */
    pl_protofd_buffer_t buffer;
    buffer.fd = writer->fd;
    buffer.base.append = fd_buffer_append;
    buffer.had_error = false;

    /* Initialize the message */
    // Plausible__Crashreport__CrashReport crashReport = PLAUSIBLE__CRASHREPORT__CRASH_REPORT__INIT;

    /* Write the message */
    // protobuf_c_message_pack_to_buffer((ProtobufCMessage *) &crashReport, (ProtobufCBuffer *) &buffer);

    return PLCRASH_ENOTSUP;
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