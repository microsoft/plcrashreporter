/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008 Landon Fuller <landonf@plausiblelabs.com>
 * Copyright (c) 2008 Plausible Labs Cooperative, Inc.
 * All rights reserved.
 */

#import <stdlib.h>
#import <fcntl.h>
#import <errno.h>
#import <string.h>
#import <stdbool.h>
#import <dlfcn.h>

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
    PLCRASH_PROTO_SYSTEM_INFO_TIMESTAMP_ID = 4,

    /** CrashReport.threads */
    PLCRASH_PROTO_BACKTRACES_ID = 2,

    /** CrashReports.thread.thread_number */
    PLCRASH_PROTO_BACKTRACE_THREAD_NUMBER_ID = 1,

    /** CrashReports.thread.frames */
    PLCRASH_PROTO_BACKTRACE_FRAMES_ID = 2,

    /** CrashReport.thread.frame.symbol_name */
    PLCRASH_PROTO_BACKTRACE_FRAME_NEAREST_SYMBOL_NAME_ID = 1,

    /** CrashReport.thread.frame.symbol_address */
    PLCRASH_PROTO_BACKTRACE_FRAME_NEAREST_SYMBOL_ADDRESS_ID = 2,
    
    /** CrashReport.thread.frame.pc */
    PLCRASH_PROTO_BACKTRACE_FRAME_PC_ID = 3
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
 * @internal
 *
 * Write a thread backtrace frame
 *
 * @param file Output file
 * @param cursor The cursor from which to acquire frame data.
 */
size_t plcrash_writer_write_backtrace_frame (plasync_file_t *file, plframe_cursor_t *cursor) {
    plframe_error_t err;
    uint64_t uint64val;
    Dl_info dlinfo;
    size_t rv = 0;

    /* PC */
    plframe_word_t pc = 0;
    if ((err = plframe_get_reg(cursor, PLFRAME_REG_IP, &pc)) != PLFRAME_ESUCCESS) {
        PLCF_DEBUG("Could not retrieve frame PC register: %s", plframe_strerror(err));
        return 0;
    }
    uint64val = pc << 4;
    PLCF_DEBUG("Want to encode pc of val: %p (%llx %llx)", (void *) pc, uint64val, (uint64val << 4));

    rv += plcrash_writer_pack(file, PLCRASH_PROTO_BACKTRACE_FRAME_PC_ID, PROTOBUF_C_TYPE_UINT64, &uint64val);

    /* Attempt to fetch symbol data (not async-safe! usually OK) */
    if (dladdr((void *) pc, &dlinfo) != 0) {
        if (dlinfo.dli_sname != NULL && dlinfo.dli_saddr != NULL) {
            PLCF_DEBUG("Packing in a symbol name: %s", dlinfo.dli_sname);
            /* Write the name */
            rv += plcrash_writer_pack(file, PLCRASH_PROTO_BACKTRACE_FRAME_NEAREST_SYMBOL_NAME_ID, PROTOBUF_C_TYPE_STRING, dlinfo.dli_sname);
            
            /* Write the address */
            uint64val = ((intptr_t) dlinfo.dli_saddr) << 4;
            rv += plcrash_writer_pack(file, PLCRASH_PROTO_BACKTRACE_FRAME_NEAREST_SYMBOL_ADDRESS_ID, PROTOBUF_C_TYPE_UINT64, &uint64val);
        } else {
            PLCF_DEBUG("Is NULL symbol");
        }
    }
    
    return rv;
}

/**
 * @internal
 *
 * Write a thread backtrace message
 *
 * @param file Output file
 * @param thread Thread for which we'll output data.
 * @param crashctx Context to use for currently running thread (rather than fetching the thread
 * context, which we've invalidated by running at all)
 */
size_t plcrash_writer_write_backtrace (plasync_file_t *file, thread_t thread, uint32_t thread_number, ucontext_t *crashctx) {
    size_t rv = 0;
    plframe_cursor_t cursor;
    plframe_error_t ferr;

    /* Write the thread ID */
    rv += plcrash_writer_pack(file, PLCRASH_PROTO_BACKTRACE_THREAD_NUMBER_ID, PROTOBUF_C_TYPE_UINT32, &thread_number);

    /* Set up the frame cursor. */
    {
        /* Use the crashctx if we're running on the crashed thread */
        thread_t thr_self= mach_thread_self();
        if (MACH_PORT_INDEX(thread) == MACH_PORT_INDEX(thr_self)) {
            ferr = plframe_cursor_init(&cursor, crashctx);
        } else {
            ferr = plframe_cursor_thread_init(&cursor, thread);
        }

        /* Did cursor initialization succeed? */
        if (ferr != PLFRAME_ESUCCESS) {
            PLCF_DEBUG("An error occured initializing the frame cursor: %s", plframe_strerror(ferr));
            return 0;
        }
    }

    /* Walk the stack */
    while ((ferr = plframe_cursor_next(&cursor)) == PLFRAME_ESUCCESS) {
        uint32_t frame_size;

        /* Determine the size */
        frame_size = plcrash_writer_write_backtrace_frame(NULL, &cursor);
        
        rv += plcrash_writer_pack(file, PLCRASH_PROTO_BACKTRACE_FRAMES_ID, PROTOBUF_C_TYPE_MESSAGE, &frame_size);
        rv += plcrash_writer_write_backtrace_frame(file, &cursor);
    }

    /* Did we reach the end successfully? */
    if (ferr != PLFRAME_ENOFRAME)
        PLCF_DEBUG("Terminated stack walking early: %s", plframe_strerror(ferr));

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
    uint32_t size;
    thread_act_array_t threads;
    mach_msg_type_number_t thread_count;

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

        /* Determine size */
        size = plcrash_writer_write_system_info(NULL, &writer->utsname, timestamp);
        
        /* Write message */
        plcrash_writer_pack(file, PLCRASH_PROTO_SYSTEM_INFO_ID, PROTOBUF_C_TYPE_MESSAGE, &size);
        plcrash_writer_write_system_info(file, &writer->utsname, timestamp);
    }

    /* Threads */
    {
        task_t self = mach_task_self();
        thread_t self_thr = mach_thread_self();

        /* Get a list of all threads */
        if (task_threads(self, &threads, &thread_count) != KERN_SUCCESS) {
            PLCF_DEBUG("Fetching thread list failed");
            thread_count = 0;
        }

        /* Suspend each thread and write out its state */
        for (mach_msg_type_number_t i = 0; i < thread_count; i++) {
            thread_t thread = threads[i];
            size_t size;
            bool suspend_thread = true;
            
            /* Check if we're running on the to be examined thread */
            if (MACH_PORT_INDEX(self_thr) == MACH_PORT_INDEX(threads[i])) {
                suspend_thread = false;
            }
            
            /* Suspend the thread */
            if (suspend_thread && thread_suspend(threads[i]) != KERN_SUCCESS) {
                PLCF_DEBUG("Could not suspend thread %d", i);
                continue;
            }
            
            /* Determine the size */
            size = plcrash_writer_write_backtrace(NULL, thread, i, crashctx);
            
            /* Write message */
            plcrash_writer_pack(file, PLCRASH_PROTO_BACKTRACES_ID, PROTOBUF_C_TYPE_MESSAGE, &size);
            plcrash_writer_write_backtrace(file, thread, i, crashctx);

            /* Resume the thread */
            if (suspend_thread)
                thread_resume(threads[i]);
        }
        
        /* Clean up the thread array */
        for (mach_msg_type_number_t i = 0; i < thread_count; i++)
            mach_port_deallocate(mach_task_self(), threads[i]);
        vm_deallocate(mach_task_self(), (vm_address_t)threads, sizeof(thread_t) * thread_count);
    }

#if 0
    
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
