/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008-2009 Plausible Labs Cooperative, Inc.
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef PLCRASH_FRAMEWALKER_H
#define PLCRASH_FRAMEWALKER_H

#import <sys/ucontext.h>
#import <pthread.h>

#import <stdint.h>
#import <stdbool.h>
#import <unistd.h>

#import <mach/mach.h>

/* Configure supported targets based on the host build architecture. There's currently
 * no deployed architecture on which simultaneous support for different processor families
 * is required (or supported), but -- in theory -- such cross-architecture support could be
 * enabled by modifying these defines. */
#if defined(__i386__) || defined(__x86_64__)
#define PLFRAME_X86_SUPPORT 1
#include <mach/i386/thread_state.h>
#endif

#if defined(__arm__)
#define PLFRAME_ARM_SUPPORT 1
#include <mach/arm/thread_state.h>
#endif

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

/**
 * Target-neutral thread-state union.
 *
 * Thread state union large enough to hold the thread state for any supported
 * architecture.
 */
typedef union plframe_cursor_thread_state {
#ifdef PLFRAME_ARM_SUPPORT
    struct {
        /** ARM thread state */
        arm_thread_state_t thread;
    } arm_state;
#endif

#ifdef PLFRAME_X86_SUPPORT
    /** Combined x86 32/64 thread state */
    struct {
        /** Thread state */
        x86_thread_state_t thread;

        /** Exception state. */
        x86_exception_state_t exception;
    } x86_state;
#endif
} plframe_thread_state_t;


/** Register number type */
typedef int plframe_regnum_t;


/**
 * General pseudo-registers common across platforms.
 *
 * Platform registers must be allocated starting at a 0
 * index, with no breaks. The following pseudo-register
 * values must be valid, and unused by other registers.
 */
typedef enum {
    /** Instruction pointer */
    PLFRAME_REG_IP = 0,
    
    /** Frame pointer */
    PLFRAME_REG_FP = 1,
} plframe_gen_regnum_t;

#import "PLCrashFrameWalker_x86.h"
#import "PLCrashFrameWalker_arm.h"

/** Platform-specific length of stack to be read when iterating frames */
#define PLFRAME_STACKFRAME_LEN PLFRAME_PDEF_STACKFRAME_LEN

/**
 * @internal
 * Frame cursor context.
 */
typedef struct plframe_cursor {
    /** The task in which the thread stack resides */
    task_t task;

    /** true if this is the initial frame */
    bool init_frame;

    /** Thread state */
    plframe_thread_state_t thread_state;

    /** Stack frame data */
    void *fp[PLFRAME_STACKFRAME_LEN];
} plframe_cursor_t;


/** Platform word type */
typedef plframe_pdef_greg_t plframe_greg_t;

/** Platform floating point register type */
typedef plframe_pdef_fpreg_t plframe_fpreg_t;


/* Shared functions */
const char *plframe_strerror (plframe_error_t error);

void plframe_thread_state_ucontext_init (plframe_thread_state_t *thread_state, ucontext_t *uap);
plframe_error_t plframe_thread_state_thread_init (plframe_thread_state_t *thread_state, thread_t thread);

plframe_error_t plframe_cursor_init (plframe_cursor_t *cursor, task_t task, plframe_thread_state_t *thread_state);
plframe_error_t plframe_cursor_signal_init (plframe_cursor_t *cursor, task_t task, ucontext_t *uap);
plframe_error_t plframe_cursor_thread_init (plframe_cursor_t *cursor, task_t task, thread_t thread);

plframe_error_t plframe_cursor_next (plframe_cursor_t *cursor);

void plframe_cursor_free(plframe_cursor_t *cursor);

/* Platform specific funtions */

/**
 * Get a register's name.
 */
char const *plframe_cursor_get_regname (plframe_cursor_t *cursor, plframe_regnum_t regnum);

/**
 * Get the total number of registers supported by the @a cursor's target thread.
 *
 * @param cursor The target cursor.
 */
size_t plframe_cursor_get_regcount (plframe_cursor_t *cursor);

/**
 * Get a register value.
 */
plframe_error_t plframe_cursor_get_reg (plframe_cursor_t *cursor, plframe_regnum_t regnum, plframe_greg_t *reg);

/**
 * Get a floating point register value.
 */
plframe_error_t plframe_cursor_get_freg (plframe_cursor_t *cursor, plframe_regnum_t regnum, plframe_fpreg_t *fpreg);

/**
 * @} plcrash_framewalker
 */

#endif /* PLCRASH_FRAMEWALKER_H */