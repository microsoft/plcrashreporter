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

#include "PLCrashASyncThread.h"

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

#import "PLCrashFrameWalker_x86.h"
#import "PLCrashFrameWalker_arm.h"

/* Supported stack direction constants. When testing PLFRAME_STACK_DIRECTION, implementors should
 * trigger an #error on unknown constants, as to make adding of new stack direction types simpler */
#define PLFRAME_STACK_DIRECTION_DOWN 1
#define PLFRAME_STACK_DIRECTION_UP 2

/* Platform-specific stack direction */
#define PLFRAME_STACK_DIRECTION PLFRAME_PDEF_STACK_DIRECTION

/** Platform-specific length of stack to be read when iterating frames */
#define PLFRAME_STACKFRAME_LEN PLFRAME_PDEF_STACKFRAME_LEN

/** The current stack frame data */
typedef struct plframe_stackframe {
    /** The frame pointer for this frame. */
    plcrash_greg_t fp;
    
    /** The PC for this frame. */
    plcrash_greg_t pc;
} plframe_stackframe_t;

/**
 * @internal
 * Frame cursor context.
 */
typedef struct plframe_cursor {
    /** The task in which the thread stack resides */
    task_t task;

    /** Thread state */
    plcrash_async_thread_state_t thread_state;
    
    /** The current frame depth. If the depth is 0, the cursor has not been stepped, and the remainder of this
     * structure should be considered uninitialized. */
    uint32_t depth;
    
    /** The previous frame. This value is unitialized if no previous frame exists (eg, a depth of <= 1) */
    plframe_stackframe_t prev_frame;

    /** The current stack frame data */
    plframe_stackframe_t frame;
} plframe_cursor_t;


/* Shared functions */
const char *plframe_strerror (plframe_error_t error);

plframe_error_t plframe_cursor_init (plframe_cursor_t *cursor, task_t task, plcrash_async_thread_state_t *thread_state);
plframe_error_t plframe_cursor_signal_init (plframe_cursor_t *cursor, task_t task, ucontext_t *uap);
plframe_error_t plframe_cursor_thread_init (plframe_cursor_t *cursor, task_t task, thread_t thread);

plframe_error_t plframe_cursor_next (plframe_cursor_t *cursor);

void plframe_cursor_free(plframe_cursor_t *cursor);

/* Platform specific funtions */

/**
 * Using the cursor's currently configured stackframe, read the next stack frame.
 *
 * @param cursor The cursor from which the frame should be read.
 * @param frame The destination frame instance.
 */
plframe_error_t plframe_cursor_read_stackframe (plframe_cursor_t *cursor, plframe_stackframe_t *frame);

/**
 * Get a register's name.
 */
char const *plframe_cursor_get_regname (plframe_cursor_t *cursor, plcrash_regnum_t regnum);

/**
 * Get the total number of registers supported by the @a cursor's target thread.
 *
 * @param cursor The target cursor.
 */
size_t plframe_cursor_get_regcount (plframe_cursor_t *cursor);

/**
 * Get a register value.
 */
plframe_error_t plframe_cursor_get_reg (plframe_cursor_t *cursor, plcrash_regnum_t regnum, plcrash_greg_t *reg);

/**
 * @} plcrash_framewalker
 */

#endif /* PLCRASH_FRAMEWALKER_H */