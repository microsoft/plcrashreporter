/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008 Plausible Labs Cooperative, Inc.
 * All rights reserved.
 */

#import <ucontext.h>
#import <pthread.h>

#import <stdint.h>
#import <stdbool.h>
#import <unistd.h>

#import <mach/mach.h>

/**
 * @internal
 * @defgroup plframe_backtrace Backtrace Frame Walker
 * @ingroup plcrash_internal
 *
 * Implements a portable backtrace API. The API is fully async safe, and may be called
 * from any signal handler.
 *
 * The API is modeled on that of the libunwind library.
 *
 * @{
 */

/**
 * Error return codes.
 */
typedef enum  {
    /** Success */
    PLFRAME_ESUCCESS = 0,

    /** Unknown error (if found, is a bug) */
    PLFRAME_EUNKNOWN,

    /** No more frames */
    PLFRAME_ENOFRAME,

    /** Bad frame */
    PLFRAME_EBADFRAME,

    /** Unsupported operation */
    PLFRAME_ENOTSUP,

    /** Invalid argument */
    PLFRAME_EINVAL,

    /** Internal error */
    PLFRAME_INTERNAL,

    /** Bad register number */
    PLFRAME_EBADREG
} plframe_error_t;


/** Register number type */
typedef int plframe_regnum_t;

#import "PLCrashFrameWalker_i386.h"
#import "PLCrashFrameWalker_arm.h"

/** Platform-specific length of stack to be read when iterating frames */
#define PLFRAME_STACKFRAME_LEN PLFRAME_PDEF_STACKFRAME_LEN

/**
 * @internal
 * Frame cursor context.
 */
typedef struct plframe_cursor {
    /** true if this is the initial frame */
    bool init_frame;
    
    /** Thread context */
    ucontext_t *uap;
    
    /** Stack frame data */
    void *fp[PLFRAME_STACKFRAME_LEN];
    
    // for thread-initialized cursors
    /** Generated ucontext_t */
    ucontext_t _uap_data;

    /** Generated mcontext_t */
    _STRUCT_MCONTEXT _mcontext_data;
} plframe_cursor_t;

/**
 * General pseudo-registers common across platforms.
 *
 * Platform registers must be allocated starting at a 0
 * index, with no breaks. The last valid register number must
 * be provided as PLFRAME_PDEF_LAST_REG.
 */
typedef enum {
    /** Instruction pointer */
    PLFRAME_REG_IP = PLFRAME_PDEF_REG_IP,
    
    /** Last register */
    PLFRAME_REG_LAST = PLFRAME_PDEF_LAST_REG
} plframe_gen_regnum_t;


/** Platform word type */
typedef plframe_pdef_reg_t plframe_reg_t;

/** Platform floating point register type */
typedef plframe_pdef_fpreg_t plframe_fpreg_t;


/**
 * @internal
 * State for test threads */
typedef struct plframe_test_thread {
    /** Running test thread */
    pthread_t thread;

    /** Thread signaling lock */
    pthread_mutex_t lock;

    /** Thread signaling (used to inform waiting callee that thread is active) */
    pthread_cond_t cond;
} plframe_test_thead_t;


/* Shared functions */
const char *plframe_strerror (plframe_error_t error);
kern_return_t plframe_read_addr (const void *source, void *dest, size_t len);

void plframe_test_thread_spawn (plframe_test_thead_t *args);
void plframe_test_thread_stop (plframe_test_thead_t *args);

/* Platform specific funtions */

/**
 * Initialize the frame cursor.
 *
 * @param cursor Cursor record to be initialized.
 * @param uap The context to use for cursor initialization.
 *
 * @return Returns PLFRAME_ESUCCESS on success, or standard plframe_error_t code if an error occurs.
 */
plframe_error_t plframe_cursor_init (plframe_cursor_t *cursor, ucontext_t *uap);

/**
 * Initialize the frame cursor by acquiring state from the provided mach thread.
 *
 * @param cursor Cursor record to be initialized.
 * @param thread The thread to use for cursor initialization.
 *
 * @return Returns PLFRAME_ESUCCESS on success, or standard plframe_error_t code if an error occurs.
 */
plframe_error_t plframe_cursor_thread_init (plframe_cursor_t *cursor, thread_t thread);

/**
 * Fetch the next cursor.
 *
 * @param cursor A cursor instance initialized with plframe_cursor_init();
 * @return Returns PLFRAME_ESUCCESS on success, PLFRAME_ENOFRAME is no additional frames are available, or a standard plframe_error_t code if an error occurs.
 */
plframe_error_t plframe_cursor_next (plframe_cursor_t *cursor);

/**
 * Get a register's name.
 */
const char *plframe_get_regname (plframe_regnum_t regnum);

/**
 * Get a register value.
 */
plframe_error_t plframe_get_reg (plframe_cursor_t *cursor, plframe_regnum_t regnum, plframe_reg_t *reg);

/**
 * Get a floating point register value.
 */
plframe_error_t plframe_get_freg (plframe_cursor_t *cursor, plframe_regnum_t regnum, plframe_fpreg_t *fpreg);

/**
 * @} plcrash_framewalker
 */