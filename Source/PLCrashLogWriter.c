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
#import "PLCrashLogWriterEncoding.h"
#import "PLCrashAsync.h"
#import "PLCrashFrameWalker.h"

#import "crash_report.pb-c.h"

/**
 * @internal
 * Protobuf Field IDs, as defined in crashreport.proto
 */
enum {
    /** CrashReport.system_info */
    PLCRASH_PROTO_SYSTEM_INFO_ID = 1,

    /** CrashReport.system_info.operating_system */
    PLCRASH_PROTO_SYSTEM_INFO_OS_ID = 1,

    /** CrashReport.system_info.os_version */
    PLCRASH_PROTO_SYSTEM_INFO_OS_VERSION_ID = 2,

    /** CrashReport.system_info.machine_type */
    PLCRASH_PROTO_SYSTEM_INFO_MACHINE_TYPE_ID = 3,

    /** CrashReport.system_info.timestamp */
    PLCRASH_PROTO_SYSTEM_INFO_TIMESTAMP_ID = 4
};

/**
 * Initialize a new crash log writer instance. This fetches all necessary environment
 * information.
 *
 * @param writer Writer instance to be initialized.
 *
 * @warning This function is not guaranteed to be async-safe, and must be
 * called prior to entering the crash handler.
 */
plcrash_error_t plcrash_writer_init (plcrash_writer_t *writer) {
    /* Fetch the OS information */
    if (uname(&writer->utsname) != 0) {
        // Should not happen
        PLCF_DEBUG("uname() failed: %s", strerror(errno));
        return PLCRASH_EINTERNAL;
    }

    return PLCRASH_ESUCCESS;
}

/**
 * @internal
 *
 * Write the system info message.
 *
 * @param writer Writer context
 * @param file Output file
 * @param timestamp Timestamp to use (seconds since epoch). Must be same across calls, as varint encoding.
 */
size_t plcrash_writer_write_system_info (plasync_file_t *file, struct utsname *utsname, uint32_t timestamp) {
    size_t rv = 0;
    uint32_t enumval;

    /* OS */
    enumval = PLCRASH_OS;
    rv += plcrash_writer_pack(file, PLCRASH_PROTO_SYSTEM_INFO_OS_ID, PROTOBUF_C_TYPE_ENUM, &enumval);

    /* OS Version */
    rv += plcrash_writer_pack(file, PLCRASH_PROTO_SYSTEM_INFO_OS_VERSION_ID, PROTOBUF_C_TYPE_STRING, utsname->release);

    /* Machine type */
    enumval = PLCRASH_MACHINE;
    rv += plcrash_writer_pack(file, PLCRASH_PROTO_SYSTEM_INFO_MACHINE_TYPE_ID, PROTOBUF_C_TYPE_ENUM, &enumval);

    /* Timestamp */
    rv += plcrash_writer_pack(file, PLCRASH_PROTO_SYSTEM_INFO_TIMESTAMP_ID, PROTOBUF_C_TYPE_UINT32, &timestamp);

    return rv;
}

/**
 * Write the crash report. All other running threads are suspended while the crash report is generated.
 *
 * @warning This method must only be called from the thread that has triggered the crash. This must correspond
 * to the provided crashctx. Failure to adhere to this requirement will result in an invalid stack trace
 * and thread dump.
 */
plcrash_error_t plcrash_writer_report (plcrash_writer_t *writer, plasync_file_t *file, siginfo_t *siginfo, ucontext_t *crashctx) {
    size_t size;

    /* System Info */
    {
        struct timeval tv;
        uint32_t timestamp;

        /* Must stay the same across both calls, so get the timestamp here */
        if (gettimeofday(&tv, NULL) != 0) {
            PLCF_DEBUG("Failed to fetch timestamp: %s", strerror(errno));
            timestamp = 0;
        } else {
            timestamp = tv.tv_sec;
        }

        // Determine size, then output
        size = plcrash_writer_write_system_info(NULL, &writer->utsname, timestamp);
        plcrash_writer_pack(file, PLCRASH_PROTO_SYSTEM_INFO_ID, PROTOBUF_C_TYPE_MESSAGE, &size);
        plcrash_writer_write_system_info(file, &writer->utsname, timestamp);
    }
    
#if 0
    Plcrash__CrashReport crashReport = PLCRASH__CRASH_REPORT__INIT;
    Plcrash__CrashReport__SystemInfo systemInfo = PLCRASH__CRASH_REPORT__SYSTEM_INFO__INIT;
    
    /* Initialize the output buffer */
    pl_protofd_buffer_t buffer;
    memset(&buffer, 0, sizeof(buffer));
    buffer.fd = writer->fd;
    buffer.base.append = fd_buffer_append;
    buffer.had_error = false;

    /* Initialize the system information */
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

    /* Threads */
    {
        crashReport.n_threads = 0;
    }
    
    /* Crashed Thread */
    // Last is the register index, so increment to get the count
    const uint32_t regCount = PLFRAME_REG_LAST + 1;
    Plcrash__CrashReport__ThreadState crashed = PLCRASH__CRASH_REPORT__THREAD_STATE__INIT;
    Plcrash__CrashReport__ThreadState__RegisterValue *crashedRegisters[regCount];
    Plcrash__CrashReport__ThreadState__RegisterValue crashedRegisterValues[regCount];
    
    crashReport.crashed_thread_state = &crashed;
    crashed.registers = crashedRegisters;
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

        // todo - thread number
        crashed.thread_number = 0;

        /* Set the register initialization value */
        Plcrash__CrashReport__ThreadState__RegisterValue init_value = PLCRASH__CRASH_REPORT__THREAD_STATE__REGISTER_VALUE__INIT;

        /* Write out registers */
        crashed.n_registers = regCount;
        for (int i = 0; i < regCount; i++) {
            
            /* Fetch the register */
            plframe_word_t regVal;
            if ((frame_err = plframe_get_reg(&cursor, i, &regVal)) != PLFRAME_ESUCCESS) {
                // Should never happen
                PLCF_DEBUG("Could not fetch register %i value: %s", i, strerror(frame_err));
                regVal = 0;
            }

            /* Initialize the register struct */
            crashedRegisterValues[i] = init_value;
            crashedRegisterValues[i].value = regVal;
            // Cast to drop 'const', since we can't make a copy.
            // The string will never be modified.
            crashedRegisterValues[i].name = (char *) plframe_get_regname(i);
            
            crashedRegisters[i] = crashedRegisterValues + i;
        }
    }

    /* Binary Images */
    {
        crashReport.n_images = 0;
    }

    protobuf_c_message_pack_to_buffer((ProtobufCMessage *) &crashReport, (ProtobufCBuffer *) &buffer);
#endif
    
    return PLCRASH_ESUCCESS;
}

/**
 * Close the plcrash_writer_t output.
 *
 * @param writer Writer instance to be closed.
 */
plcrash_error_t plcrash_writer_close (plcrash_writer_t *writer) {
    return PLCRASH_ESUCCESS;
}

/**
 * @} plcrash_log_writer
 */
