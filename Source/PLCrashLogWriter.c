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

#import <sys/time.h>

#import "PLCrashLogWriter.h"
#import "PLCrashLogWriterEncoding.h"
#import "PLCrashAsync.h"
#import "PLCrashFrameWalker.h"

#import "crash_report.pb-c.h"

static bool fetch_and_suspend_threads (thread_act_array_t *threads, mach_msg_type_number_t *thread_count);

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
    PLCRASH_PROTO_THREADS_ID = 2,

    /** CrashReports.thread.thread_number */
    PLCRASH_PROTO_THREAD_THREAD_NUMBER_ID = 1,

    /** CrashReports.thread.frames */
    PLCRASH_PROTO_THREAD_FRAMES = 2,

    /** CrashReport.thread.frame.symbol_name */
    PLCRASH_PROTO_THREAD_FRAME_SYMBOL_NAME = 1,

    /** CrashReport.thread.frame.symbol_address */
    PLCRASH_PROTO_THREAD_FRAME_SYMBOL_ADDRESS = 2,
    
    /** CrashReport.thread.frame.pc */
    PLCRASH_PROTO_THREAD_FRAME_PC = 3
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
 * Write a thread state message
 *
 * @param file Output file
 * @param thread Thread for which we'll output data.
 * @param crashctx Context to use for currently running thread (rather than fetching the thread
 * context, which we've invalidated by running at all)
 */
size_t plcrash_writer_write_thread (plasync_file_t *file, thread_t thread, uint32_t thread_number, ucontext_t *crashctx) {
    size_t rv = 0;
    plframe_cursor_t cursor;
    plframe_error_t ferr;

    /* Write the thread ID */
    rv += plcrash_writer_pack(file, PLCRASH_PROTO_THREAD_THREAD_NUMBER_ID, PROTOBUF_C_TYPE_UINT32, &thread_number);

    /* Set up the frame cursor. */
    {
        /* Use the crashctx if we're running on the crashed thread */
        thread_t thr_self= mach_thread_self();
        if (thread == thr_self) {
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
        // TODO: Output the frame record
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
    
    /* Gather and suspend all other running threads */
    if (!fetch_and_suspend_threads(&threads, &thread_count)) {
        PLCF_DEBUG("Could not suspend running threads");
        return PLCRASH_EINTERNAL;
    }

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
        /* Write out all threads */
        for (mach_msg_type_number_t i = 0; i < thread_count; i++) {
            thread_t thread = threads[i];
            size_t size;
            
            /* Determine the size */
            size = plcrash_writer_write_thread(NULL, thread, i, crashctx);
            
            /* Write message */
            plcrash_writer_pack(file, PLCRASH_PROTO_THREADS_ID, PROTOBUF_C_TYPE_MESSAGE, &size);
            plcrash_writer_write_thread(file, thread, i, crashctx);
        }
    }
    

#if 0
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
 * @internal
 *
 * Fetch all active threads and suspend them all (except for the calling thread, of course).
 *
 * Returns true on success, false on failure.
 *
 * @param threads Will be populated with a thread array. Caller is responsible for releasing
 * all returned threads, as well as the array.
 * @param 
 *
 * I originally wrote this code for OpenJDK/Mac OS X (Landon Fuller)
 */
static bool fetch_and_suspend_threads (thread_act_array_t *threads, mach_msg_type_number_t *thread_count) {
    /* Iterate over all the threads in the task, suspending each one.
     * We have to loop until no new threads appear, and all are suspended */
    mach_port_t self = mach_thread_self();
    
    mach_msg_type_number_t      cur_count, prev_count, i, k;
    thread_act_array_t          cur_list, prev_list;
    bool                        changes;
    
    changes = TRUE;
    cur_count = prev_count = 0;
    cur_list = prev_list = NULL;
    do {
        /* Get a list of all threads */
        if (task_threads(mach_task_self(), &cur_list, &cur_count) != KERN_SUCCESS) {
            PLCF_DEBUG("Fetching thread list failed");
            goto error;
        }
        
        /* For each thread, check if it was previously suspended. If it
         * was not, suspend it now, and set the changes flag to 'true' */
        changes = FALSE;
        for (i = 0; i < cur_count; i++) {
            mach_msg_type_number_t j;
            bool found = FALSE;
            
            /* Check the previous thread list */
            for (j = 0; j < prev_count; j++) {
                if (prev_list[j] == cur_list[i]) {
                    found = TRUE;
                    break;
                }
            }
            
            /* If the thread wasn't previously suspended, suspend it now and set the change flag */
            if (found) {
                /* Don't suspend ourselves! */
                if (cur_list[i] != self)
                    thread_suspend(cur_list[i]);
                changes = TRUE;
            }
        }
        
        /* Deallocate the previous list, if necessary */
        for (k = 0; k < prev_count; k++)
            mach_port_deallocate(mach_task_self(), prev_list[k]);
        
        vm_deallocate(mach_task_self(), (vm_address_t)prev_list, sizeof(thread_t) * prev_count);
        
        /* Set up the 'new' list for the next loop iteration */
        prev_list = cur_list;
        prev_count = cur_count;
    } while (changes);

    /* Success */
    *threads = cur_list;
    *thread_count = cur_count;
    return true;

error:
    /* Clean up, if necessary */
    if (prev_count > 0) {
        for (i = 0; i < prev_count; i++)
            mach_port_deallocate(self, prev_list[i]);

        vm_deallocate(self, (vm_address_t)prev_list, sizeof(thread_t) * prev_count);
    }

    return false;
}


/**
 * @} plcrash_log_writer
 */
