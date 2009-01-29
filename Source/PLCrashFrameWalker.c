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


#import "PLCrashFrameWalker.h"


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


/**
 * (Safely) read len bytes from addr, storing in dest. Uses mach vm_read_overwrite to
 * avoid dereferencing a bad pointer.
 */
kern_return_t plframe_read_addr (const void *source, void *dest, size_t len) {
    vm_size_t read_size = len;
    return vm_read_overwrite(mach_task_self(), (vm_address_t) source, len, (pointer_t) dest, &read_size);
}

/* A thread that exists just to give us a stack to iterate */
static void *test_stack_thr (void *arg) {
    plframe_test_thead_t *args = arg;
    
    /* Acquire the lock and inform our caller that we're active */
    pthread_mutex_lock(&args->lock);
    pthread_cond_signal(&args->cond);
    
    /* Wait for a shut down request, and then drop the acquired lock immediately */
    pthread_cond_wait(&args->cond, &args->lock);
    pthread_mutex_unlock(&args->lock);
    
    return NULL;
}


/** Spawn a test thread that may be used as an iterable stack. (For testing only!) */
void plframe_test_thread_spawn (plframe_test_thead_t *args) {
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
void plframe_test_thread_stop (plframe_test_thead_t *args) {
    /* Signal the thread to exit */
    pthread_mutex_lock(&args->lock);
    pthread_cond_signal(&args->cond);
    pthread_mutex_unlock(&args->lock);
    
    /* Wait for exit */
    pthread_join(args->thread, NULL);
}
