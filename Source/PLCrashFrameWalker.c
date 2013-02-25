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

#include "PLCrashFrameWalker.h"
#include "PLCrashAsync.h"
#include "PLCrashFrameStackUnwind.h"
#include "PLCrashTestThread.h"

#pragma mark Error Handling

/**
 * Return an error description for the given plframe_error_t.
 */
const char *plframe_strerror (plframe_error_t error) {
    switch (error) {
        case PLFRAME_ESUCCESS:
            return "No error";
        case PLFRAME_EUNKNOWN:
            return "Unknown error";
        case PLFRAME_ENOFRAME:
            return "No frames are available";
        case PLFRAME_EBADFRAME:
            return "Corrupted frame";
        case PLFRAME_ENOTSUP:
            return "Operation not supported";
        case PLFRAME_EINVAL:
            return "Invalid argument";
        case PLFRAME_INTERNAL:
            return "Internal error";
        case PLFRAME_EBADREG:
            return "Invalid register";
    }

    /* Should be unreachable */
    return "Unhandled error code";
}

#pragma mark Test Thread

/* A thread that exists just to give us a stack to iterate */
static void *test_stack_thr (void *arg) {
    plcrash_test_thread_t *args = arg;
    
    /* Acquire the lock and inform our caller that we're active */
    pthread_mutex_lock(&args->lock);
    pthread_cond_signal(&args->cond);
    
    /* Wait for a shut down request, and then drop the acquired lock immediately */
    pthread_cond_wait(&args->cond, &args->lock);
    pthread_mutex_unlock(&args->lock);
    
    return NULL;
}


/** Spawn a test thread that may be used as an iterable stack. (For testing only!) */
void plframe_test_thread_spawn (plcrash_test_thread_t *args) {
    /* Initialize the args */
    pthread_mutex_init(&args->lock, NULL);
    pthread_cond_init(&args->cond, NULL);
    
    /* Lock and start the thread */
    pthread_mutex_lock(&args->lock);
    pthread_create(&args->thread, NULL, test_stack_thr, args);
    pthread_cond_wait(&args->cond, &args->lock);
    pthread_mutex_unlock(&args->lock);
}

/** Stop a test thread. */
void plframe_test_thread_stop (plcrash_test_thread_t *args) {
    /* Signal the thread to exit */
    pthread_mutex_lock(&args->lock);
    pthread_cond_signal(&args->cond);
    pthread_mutex_unlock(&args->lock);
    
    /* Wait for exit */
    pthread_join(args->thread, NULL);
}

#pragma mark Frame Walking

/**
 * @internal
 * Shared initializer
 */
static void plframe_cursor_internal_init (plframe_cursor_t *cursor, task_t task) {
    cursor->init_frame = true;
    cursor->fp[0] = NULL;
    cursor->task = task;
    mach_port_mod_refs(mach_task_self(), cursor->task, MACH_PORT_RIGHT_SEND, 1);
}

/**
 * Initialize the frame cursor using the provided thread state.
 *
 * @param cursor Cursor record to be initialized.
 * @param task The task from which @a uap was derived. All memory will be mapped from this task.
 * @param thread_state The thread state to use for cursor initialization.
 *
 * @return Returns PLFRAME_ESUCCESS on success, or standard plframe_error_t code if an error occurs.
 *
 * @warn Callers must call plframe_cursor_free() on @a cursor to free any associated resources, even if initialization
 * fails.
 */
plframe_error_t plframe_cursor_init (plframe_cursor_t *cursor, task_t task, plcrash_async_thread_state_t *thread_state) {
    plframe_cursor_internal_init(cursor, task);
    plcrash_async_memcpy(&cursor->thread_state, thread_state, sizeof(cursor->thread_state));
    
    return PLFRAME_ESUCCESS;
}

/**
 * Initialize the frame cursor using a signal-provided context;
 *
 * @param cursor Cursor record to be initialized.
 * @param task The task from which @a uap was derived. All memory will be mapped from this task.
 * @param uap The context to use for cursor initialization.
 *
 * @return Returns PLFRAME_ESUCCESS on success, or standard plframe_error_t code if an error occurs.
 *
 * @warn Callers must call plframe_cursor_free() on @a cursor to free any associated resources, even if initialization
 * fails.
 */
plframe_error_t plframe_cursor_signal_init (plframe_cursor_t *cursor, task_t task, ucontext_t *uap) {
    /* Standard initialization */
    plframe_cursor_internal_init(cursor, task);

    plcrash_async_thread_state_ucontext_init(&cursor->thread_state, uap);

    return PLFRAME_ESUCCESS;
}

/**
 * Initialize the frame cursor by acquiring state from the provided mach thread. If the thread is not suspended,
 * the fetched state may be inconsistent.
 *
 * @param cursor Cursor record to be initialized.
 * @param task The task in which @a thread is running. All memory will be mapped from this task.
 * @param thread The thread to use for cursor initialization.
 *
 * @return Returns PLFRAME_ESUCCESS on success, or standard plframe_error_t code if an error occurs.
 *
 * @warn Callers must call plframe_cursor_free() on @a cursor to free any associated resources, even if initialization
 * fails.
 */
plframe_error_t plframe_cursor_thread_init (plframe_cursor_t *cursor, task_t task, thread_t thread) {
    /* Standard initialization */
    plframe_cursor_internal_init(cursor, task);

    return plcrash_async_thread_state_mach_thread_init(&cursor->thread_state, thread);
}


/**
 * Fetch the next frame.
 *
 * @param cursor A cursor instance initialized with plframe_cursor_init();
 * @return Returns PLFRAME_ESUCCESS on success, PLFRAME_ENOFRAME is no additional frames are available, or a standard plframe_error_t code if an error occurs.
 */
plframe_error_t plframe_cursor_next (plframe_cursor_t *cursor) {
    return plframe_cursor_next_fp(cursor);
}

/**
 * Free any resources associated with the frame cursor.
 *
 * @param cursor Cursor record to be freed
 */
void plframe_cursor_free(plframe_cursor_t *cursor) {
    if (cursor->task != MACH_PORT_NULL)
        mach_port_mod_refs(mach_task_self(), cursor->task, MACH_PORT_RIGHT_SEND, -1);
}
