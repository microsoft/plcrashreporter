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

#import "crash_report.pb-c.h"

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

struct M {
    Plcrash__CrashReport crashReport;
    Plcrash__CrashReport__SystemInfo systemInfo;
    Plcrash__CrashReport__ThreadState threadState;
};

/**
 * Write the crash report. All other running threads are suspended while the crash report is generated.
 *
 * @warning This method must only be called from the thread that has triggered the crash. This must correspond
 * to the provided crashctx. Failure to adhere to this requirement will result in an invalid stack trace
 * and thread dump.
 */
plcrash_error_t plcrash_writer_report (plcrash_writer_t *writer, siginfo_t *siginfo, ucontext_t *crashctx) {

    
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
