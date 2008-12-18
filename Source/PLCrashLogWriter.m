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

#import "PLCrashLog.h"
#import "PLCrashLogWriter.h"
#import "PLCrashLogWriterEncoding.h"
#import "PLCrashAsync.h"
#import "PLCrashAsyncSignalInfo.h"
#import "PLCrashFrameWalker.h"

#if TARGET_OS_IPHONE
#import <UIKit/UIKit.h> // For UIDevice
#endif

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

    /** CrashReport.system_info.architecture */
    PLCRASH_PROTO_SYSTEM_INFO_ARCHITECTURE_TYPE_ID = 3,

    /** CrashReport.system_info.timestamp */
    PLCRASH_PROTO_SYSTEM_INFO_TIMESTAMP_ID = 4,

    /** CrashReport.app_info */
    PLCRASH_PROTO_APP_INFO_ID = 2,
    
    /** CrashReport.app_info.app_identifier */
    PLCRASH_PROTO_APP_INFO_APP_IDENTIFIER_ID = 1,
    
    /** CrashReport.app_info.app_version */
    PLCRASH_PROTO_APP_INFO_APP_VERSION_ID = 2,


    /** CrashReport.threads */
    PLCRASH_PROTO_THREADS_ID = 3,
    

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

    
    /** CrashReport.exception */
    PLCRASH_PROTO_EXCEPTION_ID = 5,

    /** CrashReport.exception.name */
    PLCRASH_PROTO_EXCEPTION_NAME_ID = 1,
    
    /** CrashReport.exception.reason */
    PLCRASH_PROTO_EXCEPTION_REASON_ID = 2,

    /** CrashReport.signal */
    PLCRASH_PROTO_SIGNAL_ID = 6,

    /** CrashReport.signal.name */
    PLCRASH_PROTO_SIGNAL_NAME_ID = 1,

    /** CrashReport.signal.code */
    PLCRASH_PROTO_SIGNAL_CODE_ID = 2,
    
    /** CrashReport.signal.address */
    PLCRASH_PROTO_SIGNAL_ADDRESS_ID = 3,
};


/**
 * Initialize a new crash log writer instance. This fetches all necessary environment
 * information.
 *
 * @param writer Writer instance to be initialized.
 * @param app_identifier Unique per-application identifier. On Mac OS X, this is likely the CFBundleIdentifier.
 * @param app_version Application version string.
 *
 * @note If this function fails, plcrash_log_writer_free() should be called
 * to free any partially allocated data.
 *
 * @warning This function is not guaranteed to be async-safe, and must be
 * called prior to entering the crash handler.
 */
plcrash_error_t plcrash_log_writer_init (plcrash_log_writer_t *writer, NSString *app_identifier, NSString *app_version) {
    /* Default to 0 */
    memset(writer, 0, sizeof(*writer));
    
    /* Fetch the application information */
    {
        writer->application_info.app_identifier = strdup([app_identifier UTF8String]);
        writer->application_info.app_version = strdup([app_version UTF8String]);
    }

    /* Fetch the OS information */
#if TARGET_OS_IPHONE
    /* iPhone OS */
    writer->system_info.version = strdup([[[UIDevice currentDevice] systemVersion] UTF8String]);
#elif TARGET_OS_MAC
    /* Mac OS X */
    {
        SInt32 major, minor, bugfix;

        /* Fetch the major, minor, and bugfix versions.
         * Fetching the OS version should not fail. */
        if (Gestalt(gestaltSystemVersionMajor, &major) != noErr) {
            PLCF_DEBUG("Could not retreive system major version with Gestalt");
            return PLCRASH_EINTERNAL;
        }
        if (Gestalt(gestaltSystemVersionMinor, &minor) != noErr) {
            PLCF_DEBUG("Could not retreive system minor version with Gestalt");
            return PLCRASH_EINTERNAL;
        }
        if (Gestalt(gestaltSystemVersionBugFix, &bugfix) != noErr) {
            PLCF_DEBUG("Could not retreive system bugfix version with Gestalt");
            return PLCRASH_EINTERNAL;
        }

        /* Compose the string */
        asprintf(&writer->system_info.version, "%d.%d.%d", major, minor, bugfix);
    }
#else
#error Unsupported Platform
#endif

    return PLCRASH_ESUCCESS;
}

/**
 * Set the uncaught exception for this writer. Once set, this exception will be used to
 * provide exception data for the crash log output.
 *
 * @warning This function is not async safe, and must be called outside of a signal handler.
 */
void plcrash_log_writer_set_exception (plcrash_log_writer_t *writer, NSException *exception) {
    assert(writer->uncaught_exception.has_exception == false);

    /* Save the exception data */
    writer->uncaught_exception.has_exception = true;
    writer->uncaught_exception.name = strdup([[exception name] UTF8String]);
    writer->uncaught_exception.reason = strdup([[exception reason] UTF8String]);
}

/**
 * Close the plcrash_writer_t output.
 *
 * @param writer Writer instance to be closed.
 */
plcrash_error_t plcrash_log_writer_close (plcrash_log_writer_t *writer) {
    return PLCRASH_ESUCCESS;
}

/**
 * Free any crash log writer resources.
 *
 * @warning This method is not async safe.
 */
void plcrash_log_writer_free (plcrash_log_writer_t *writer) {
    /* Free the app info */
    if (writer->application_info.app_identifier != NULL)
        free(writer->application_info.app_identifier);
    if (writer->application_info.app_version != NULL)
        free(writer->application_info.app_version);

    /* Free the system info */
    if (writer->system_info.version != NULL)
        free(writer->system_info.version);

    /* Free the exception data */
    if (writer->uncaught_exception.has_exception) {
        if (writer->uncaught_exception.name != NULL)
            free(writer->uncaught_exception.name);

        if (writer->uncaught_exception.reason != NULL)
            free(writer->uncaught_exception.reason);
    }
}

/**
 * @internal
 *
 * Write the system info message.
 *
 * @param file Output file
 * @param timestamp Timestamp to use (seconds since epoch). Must be same across calls, as varint encoding.
 */
static size_t plcrash_writer_write_system_info (plcrash_async_file_t *file, plcrash_log_writer_t *writer, uint32_t timestamp) {
    size_t rv = 0;
    uint32_t enumval;

    /* OS */
    enumval = PLCrashLogHostOperatingSystem;
    rv += plcrash_writer_pack(file, PLCRASH_PROTO_SYSTEM_INFO_OS_ID, PLPROTOBUF_C_TYPE_ENUM, &enumval);

    /* OS Version */
    rv += plcrash_writer_pack(file, PLCRASH_PROTO_SYSTEM_INFO_OS_VERSION_ID, PLPROTOBUF_C_TYPE_STRING, writer->system_info.version);

    /* Machine type */
    enumval = PLCrashLogHostArchitecture;
    rv += plcrash_writer_pack(file, PLCRASH_PROTO_SYSTEM_INFO_ARCHITECTURE_TYPE_ID, PLPROTOBUF_C_TYPE_ENUM, &enumval);

    /* Timestamp */
    rv += plcrash_writer_pack(file, PLCRASH_PROTO_SYSTEM_INFO_TIMESTAMP_ID, PLPROTOBUF_C_TYPE_UINT32, &timestamp);

    return rv;
}

/**
 * @internal
 *
 * Write the app info message.
 *
 * @param file Output file
 * @param app_identifier Application identifier
 * @param app_version Application version
 */
static size_t plcrash_writer_write_app_info (plcrash_async_file_t *file, const char *app_identifier, const char *app_version) {
    size_t rv = 0;

    /* App identifier */
    rv += plcrash_writer_pack(file, PLCRASH_PROTO_APP_INFO_APP_IDENTIFIER_ID, PLPROTOBUF_C_TYPE_STRING, app_identifier);
    
    /* App version */
    rv += plcrash_writer_pack(file, PLCRASH_PROTO_APP_INFO_APP_VERSION_ID, PLPROTOBUF_C_TYPE_STRING, app_version);
    
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
static size_t plcrash_writer_write_thread_register (plcrash_async_file_t *file, const char *regname, plframe_greg_t regval) {
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
static size_t plcrash_writer_write_thread_registers (plcrash_async_file_t *file, ucontext_t *uap) {
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
static size_t plcrash_writer_write_thread_frame (plcrash_async_file_t *file, plframe_cursor_t *cursor) {
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
static size_t plcrash_writer_write_thread (plcrash_async_file_t *file, thread_t thread, uint32_t thread_number, ucontext_t *crashctx) {
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
static size_t plcrash_writer_write_binary_image (plcrash_async_file_t *file, const char *name, const struct mach_header *header) {
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
 * @internal
 *
 * Write the crash Exception message
 *
 * @param file Output file
 * @param writer Writer containing exception data
 */
static size_t plcrash_writer_write_exception (plcrash_async_file_t *file, plcrash_log_writer_t *writer) {
    size_t rv = 0;

    /* Write the name and reason */
    assert(writer->uncaught_exception.has_exception);
    rv += plcrash_writer_pack(file, PLCRASH_PROTO_EXCEPTION_NAME_ID, PLPROTOBUF_C_TYPE_STRING, writer->uncaught_exception.name);
    rv += plcrash_writer_pack(file, PLCRASH_PROTO_EXCEPTION_REASON_ID, PLPROTOBUF_C_TYPE_STRING, writer->uncaught_exception.reason);

    return rv;
}

/**
 * @internal
 *
 * Write the crash signal message
 *
 * @param file Output file
 * @param siginfo The signal information
 */
static size_t plcrash_writer_write_signal (plcrash_async_file_t *file, siginfo_t *siginfo) {
    size_t rv = 0;
    
    /* Fetch the signal name */
    char name_buf[10];
    const char *name;
    if ((name = plcrash_async_signal_signame(siginfo->si_signo)) == NULL) {
        PLCF_DEBUG("Warning -- unhandled signal number (signo=%d). This is a bug.", siginfo->si_signo);
        snprintf(name_buf, sizeof(name_buf), "#%d", siginfo->si_signo);
        name = name_buf;
    }

    /* Fetch the signal code string */
    char code_buf[10];
    const char *code;
    if ((code = plcrash_async_signal_sigcode(siginfo->si_signo, siginfo->si_code)) == NULL) {
        PLCF_DEBUG("Warning -- unhandled signal sicode (signo=%d, code=%d). This is a bug.", siginfo->si_signo, siginfo->si_code);
        snprintf(code_buf, sizeof(code_buf), "#%d", siginfo->si_code);
        code = code_buf;
    }
    
    /* Address value */
    uint64_t addr = (intptr_t) siginfo->si_addr;

    /* Write it out */
    rv += plcrash_writer_pack(file, PLCRASH_PROTO_SIGNAL_NAME_ID, PLPROTOBUF_C_TYPE_STRING, name);
    rv += plcrash_writer_pack(file, PLCRASH_PROTO_SIGNAL_CODE_ID, PLPROTOBUF_C_TYPE_STRING, code);
    rv += plcrash_writer_pack(file, PLCRASH_PROTO_SIGNAL_ADDRESS_ID, PLPROTOBUF_C_TYPE_UINT64, &addr);

    return rv;
}

/**
 * Write the crash report. All other running threads are suspended while the crash report is generated.
 *
 * @param writer The writer context
 * @param file The output file.
 * @param siginfo Signal information
 * @param crashctx Context of the crashed thread.
 *
 * @warning This method must only be called from the thread that has triggered the crash. This must correspond
 * to the provided crashctx. Failure to adhere to this requirement will result in an invalid stack trace
 * and thread dump.
 */
plcrash_error_t plcrash_log_writer_write (plcrash_log_writer_t *writer, plcrash_async_file_t *file, siginfo_t *siginfo, ucontext_t *crashctx) {
    thread_act_array_t threads;
    mach_msg_type_number_t thread_count;

    /* File header */
    {
        uint8_t version = PLCRASH_LOG_FILE_VERSION;

        /* Write the magic string (with no trailing NULL) and the version number */
        plcrash_async_file_write(file, PLCRASH_LOG_FILE_MAGIC, strlen(PLCRASH_LOG_FILE_MAGIC));
        plcrash_async_file_write(file, &version, sizeof(version));
    }

    /* System Info */
    {
        struct timeval tv;
        uint32_t timestamp;
        uint32_t size;

        /* Must stay the same across both calls, so get the timestamp here */
        if (gettimeofday(&tv, NULL) != 0) {
            PLCF_DEBUG("Failed to fetch timestamp: %s", strerror(errno));
            timestamp = 0;
        } else {
            timestamp = tv.tv_sec;
        }

        /* Determine size */
        size = plcrash_writer_write_system_info(NULL, writer, timestamp);
        
        /* Write message */
        plcrash_writer_pack(file, PLCRASH_PROTO_SYSTEM_INFO_ID, PLPROTOBUF_C_TYPE_MESSAGE, &size);
        plcrash_writer_write_system_info(file, writer, timestamp);
    }
    
    /* App info */
    {
        uint32_t size;

        /* Determine size */
        size = plcrash_writer_write_app_info(NULL, writer->application_info.app_identifier, writer->application_info.app_version);
        
        /* Write message */
        plcrash_writer_pack(file, PLCRASH_PROTO_APP_INFO_ID, PLPROTOBUF_C_TYPE_MESSAGE, &size);
        plcrash_writer_write_app_info(file, writer->application_info.app_identifier, writer->application_info.app_version);
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

    /* Exception */
    if (writer->uncaught_exception.has_exception) {
        uint32_t size;

        /* Calculate the message size */
        size = plcrash_writer_write_exception(NULL, writer);
        plcrash_writer_pack(file, PLCRASH_PROTO_EXCEPTION_ID, PLPROTOBUF_C_TYPE_MESSAGE, &size);
        plcrash_writer_write_exception(file, writer);
    }
    
    /* Signal */
    {
        uint32_t size;
        
        /* Calculate the message size */
        size = plcrash_writer_write_signal(NULL, siginfo);
        plcrash_writer_pack(file, PLCRASH_PROTO_SIGNAL_ID, PLPROTOBUF_C_TYPE_MESSAGE, &size);
        plcrash_writer_write_signal(file, siginfo);
    }
    
    return PLCRASH_ESUCCESS;
}


/**
 * @} plcrash_log_writer
 */
