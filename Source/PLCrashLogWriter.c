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

#import <mach-o/dyld.h>

#import "PLCrashLogWriter.h"
#import "PLCrashLogWriterEncoding.h"
#import "PLCrashAsync.h"
#import "PLCrashFrameWalker.h"
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
    PLCRASH_PROTO_THREAD_FRAMES_ID = 2,
    
    /** CrashReport.thread.frame.pc */
    PLCRASH_PROTO_THREAD_FRAME_PC_ID = 3,

    /** CrashReport.thread.crashed */
    PLCRASH_PROTO_THREAD_CRASHED_ID = 3,



    /** CrashReport.thread.registers */
    PLCRASH_PROTO_THREAD_REGISTERS_ID = 4,

    /** CrashReport.thread.register.name */
    PLCRASH_PROTO_THREAD_REGISTER_NAME_ID = 1,

    /** CrashReport.thread.register.name */
    PLCRASH_PROTO_THREAD_REGISTER_VALUE_ID = 2,


    /** CrashReport.images */
    PLCRASH_PROTO_BINARY_IMAGES_ID = 4,

    /** CrashReport.BinaryImage.base_address */
    PLCRASH_PROTO_BINARY_IMAGE_ADDR_ID = 1,

    /** CrashReport.BinaryImage.size */
    PLCRASH_PROTO_BINARY_IMAGE_SIZE_ID = 2,

    /** CrashReport.BinaryImage.name */
    PLCRASH_PROTO_BINARY_IMAGE_NAME_ID = 3,
    
    /** CrashReport.BinaryImage.uuid */
    PLCRASH_PROTO_BINARY_IMAGE_UUID_ID = 4,
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
    rv += plcrash_writer_pack(file, PLCRASH_PROTO_SYSTEM_INFO_OS_ID, PLPROTOBUF_C_TYPE_ENUM, &enumval);

    /* OS Version */
    rv += plcrash_writer_pack(file, PLCRASH_PROTO_SYSTEM_INFO_OS_VERSION_ID, PLPROTOBUF_C_TYPE_STRING, utsname->release);

    /* Machine type */
    enumval = PLCRASH_MACHINE;
    rv += plcrash_writer_pack(file, PLCRASH_PROTO_SYSTEM_INFO_MACHINE_TYPE_ID, PLPROTOBUF_C_TYPE_ENUM, &enumval);

    /* Timestamp */
    rv += plcrash_writer_pack(file, PLCRASH_PROTO_SYSTEM_INFO_TIMESTAMP_ID, PLPROTOBUF_C_TYPE_UINT32, &timestamp);

    return rv;
}


/**
 * @internal
 *
 * Write a thread backtrace register
 *
 * @param file Output file
 * @param cursor The cursor from which to acquire frame data.
 */
size_t plcrash_writer_write_thread_register (plasync_file_t *file, const char *regname, plframe_greg_t regval) {
    uint64_t uint64val;
    size_t rv = 0;

    /* Write the name */
    rv += plcrash_writer_pack(file, PLCRASH_PROTO_THREAD_REGISTER_NAME_ID, PLPROTOBUF_C_TYPE_STRING, regname);

    /* Write the value */
    uint64val = regval;
    rv += plcrash_writer_pack(file, PLCRASH_PROTO_THREAD_REGISTER_VALUE_ID, PLPROTOBUF_C_TYPE_UINT64, &uint64val);
    
    return rv;
}

/**
 * @internal
 *
 * Write all thread backtrace register messages
 *
 * @param file Output file
 * @param cursor The cursor from which to acquire frame data.
 */
size_t plcrash_writer_write_thread_registers (plasync_file_t *file, ucontext_t *uap) {
    plframe_cursor_t cursor;
    plframe_error_t frame_err;
    uint32_t regCount;
    size_t rv = 0;

    /* Last is an index value, so increment to get the count */
    regCount = PLFRAME_REG_LAST + 1;

    /* Create the crashed thread frame cursor */
    if ((frame_err = plframe_cursor_init(&cursor, uap)) != PLFRAME_ESUCCESS) {
        PLCF_DEBUG("Failed to initialize frame cursor for crashed thread: %s", plframe_strerror(frame_err));
        return 0;
    }
    
    /* Fetch the first frame */
    if ((frame_err = plframe_cursor_next(&cursor)) != PLFRAME_ESUCCESS) {
        PLCF_DEBUG("Could not fetch crashed thread frame: %s", plframe_strerror(frame_err));
        return 0;
    }
    
    /* Write out register messages */
    for (int i = 0; i < regCount; i++) {
        plframe_greg_t regVal;
        const char *regname;
        uint32_t msgsize;

        /* Fetch the register value */
        if ((frame_err = plframe_get_reg(&cursor, i, &regVal)) != PLFRAME_ESUCCESS) {
            // Should never happen
            PLCF_DEBUG("Could not fetch register %i value: %s", i, strerror(frame_err));
            regVal = 0;
        }

        /* Fetch the register name */
        regname = plframe_get_regname(i);

        /* Get the register message size */
        msgsize = plcrash_writer_write_thread_register(NULL, regname, regVal);
        
        /* Write the header and message */
        rv += plcrash_writer_pack(file, PLCRASH_PROTO_THREAD_REGISTERS_ID, PLPROTOBUF_C_TYPE_MESSAGE, &msgsize);
        rv += plcrash_writer_write_thread_register(file, regname, regVal);
    }
    
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
size_t plcrash_writer_write_thread_frame (plasync_file_t *file, plframe_cursor_t *cursor) {
    plframe_error_t err;
    uint64_t uint64val;
    size_t rv = 0;

    /* PC */
    plframe_greg_t pc = 0;
    if ((err = plframe_get_reg(cursor, PLFRAME_REG_IP, &pc)) != PLFRAME_ESUCCESS) {
        PLCF_DEBUG("Could not retrieve frame PC register: %s", plframe_strerror(err));
        return 0;
    }
    uint64val = pc;
    rv += plcrash_writer_pack(file, PLCRASH_PROTO_THREAD_FRAME_PC_ID, PLPROTOBUF_C_TYPE_UINT64, &uint64val);

    return rv;
}

/**
 * @internal
 *
 * Write a thread message
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
    bool crashed_thread = false;

    /* Write the thread ID */
    rv += plcrash_writer_pack(file, PLCRASH_PROTO_THREAD_THREAD_NUMBER_ID, PLPROTOBUF_C_TYPE_UINT32, &thread_number);
    
    /* Is this the crashed thread? */
    thread_t thr_self = mach_thread_self();
    if (MACH_PORT_INDEX(thread) == MACH_PORT_INDEX(thr_self))
        crashed_thread = true;

    /* Set up the frame cursor. */
    {
        /* Use the crashctx if we're running on the crashed thread */
        if (crashed_thread) {
            ferr = plframe_cursor_init(&cursor, crashctx);
            crashed_thread = true;
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
        frame_size = plcrash_writer_write_thread_frame(NULL, &cursor);
        
        rv += plcrash_writer_pack(file, PLCRASH_PROTO_THREAD_FRAMES_ID, PLPROTOBUF_C_TYPE_MESSAGE, &frame_size);
        rv += plcrash_writer_write_thread_frame(file, &cursor);
    }

    /* Did we reach the end successfully? */
    if (ferr != PLFRAME_ENOFRAME)
        PLCF_DEBUG("Terminated stack walking early: %s", plframe_strerror(ferr));

    /* Note crashed status */
    rv += plcrash_writer_pack(file, PLCRASH_PROTO_THREAD_CRASHED_ID, PLPROTOBUF_C_TYPE_BOOL, &crashed_thread);
    
    /* Dump registers for the crashed thread */
    if (crashed_thread) {
        rv += plcrash_writer_write_thread_registers(file, crashctx);
    }

    return rv;
}


/**
 * @internal
 *
 * Write a binary image frame
 *
 * @param file Output file
 * @param cursor The cursor from which to acquire frame data.
 */
size_t plcrash_writer_write_binary_image (plasync_file_t *file, const char *name, const struct mach_header *header) {
    size_t rv = 0;
    uint64_t mach_size = 0;

    /* Compute the image size and search for a UUID */
    struct load_command *cmd = (struct load_command *) (header + 1);
    struct uuid_command *uuid = NULL;

    for (uint32_t i = 0; cmd != NULL && i < header->ncmds; i++) {
        /* 32-bit text segment */
        if (cmd->cmd == LC_SEGMENT) {
            struct segment_command *segment = (struct segment_command *) cmd;
            if (strcmp(segment->segname, SEG_TEXT) == 0) {
                mach_size = segment->vmsize;
            }
        }
        /* 64-bit text segment */
        else if (cmd->cmd == LC_SEGMENT_64) {
            struct segment_command_64 *segment = (struct segment_command_64 *) cmd;

            if (strcmp(segment->segname, SEG_TEXT) == 0) {
                mach_size = segment->vmsize;
            }
        }
        /* DWARF dSYM UUID */
        else if (cmd->cmd == LC_UUID && cmd->cmdsize == sizeof(struct uuid_command)) {
            uuid = (struct uuid_command *) cmd;
        }

        cmd = (struct load_command *) ((uint8_t *) cmd + cmd->cmdsize);
    }

    rv += plcrash_writer_pack(file, PLCRASH_PROTO_BINARY_IMAGE_SIZE_ID, PLPROTOBUF_C_TYPE_UINT64, &mach_size);
    
    /* Base address */
    {
        uintptr_t base_addr;
        uint64_t u64;

        base_addr = (uintptr_t) header;
        u64 = base_addr;
        rv += plcrash_writer_pack(file, PLCRASH_PROTO_BINARY_IMAGE_ADDR_ID, PLPROTOBUF_C_TYPE_UINT64, &u64);
    }

    /* Name */
    rv += plcrash_writer_pack(file, PLCRASH_PROTO_BINARY_IMAGE_NAME_ID, PLPROTOBUF_C_TYPE_STRING, name);

    /* UUID */
    if (uuid != NULL) {
        PLProtobufCBinaryData binary;
    
        /* Write the 128-bit UUID */
        binary.len = sizeof(uuid->uuid);
        binary.data = uuid->uuid;
        rv += plcrash_writer_pack(file, PLCRASH_PROTO_BINARY_IMAGE_UUID_ID, PLPROTOBUF_C_TYPE_BYTES, &binary);
    }

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
        plcrash_writer_pack(file, PLCRASH_PROTO_SYSTEM_INFO_ID, PLPROTOBUF_C_TYPE_MESSAGE, &size);
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
            uint32_t size;
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
            size = plcrash_writer_write_thread(NULL, thread, i, crashctx);
            
            /* Write message */
            plcrash_writer_pack(file, PLCRASH_PROTO_THREADS_ID, PLPROTOBUF_C_TYPE_MESSAGE, &size);
            plcrash_writer_write_thread(file, thread, i, crashctx);

            /* Resume the thread */
            if (suspend_thread)
                thread_resume(threads[i]);
        }
        
        /* Clean up the thread array */
        for (mach_msg_type_number_t i = 0; i < thread_count; i++)
            mach_port_deallocate(mach_task_self(), threads[i]);
        vm_deallocate(mach_task_self(), (vm_address_t)threads, sizeof(thread_t) * thread_count);
    }

    /* Binary Images */
    uint32_t image_count = _dyld_image_count();
    for (uint32_t i = 0; i < image_count; i++) {
        const struct mach_header *header;
        const char *name;
        uint32_t size;

        /* Fetch the info */
        header = _dyld_get_image_header(i);
        name = _dyld_get_image_name(i);

        /* Calculate the message size */
        size = plcrash_writer_write_binary_image(NULL, name, header);
        plcrash_writer_pack(file, PLCRASH_PROTO_BINARY_IMAGES_ID, PLPROTOBUF_C_TYPE_MESSAGE, &size);
        plcrash_writer_write_binary_image(file, name, header);
    }
    
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
