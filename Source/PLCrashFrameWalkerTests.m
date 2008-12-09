/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008 Plausible Labs Cooperative, Inc.
 * All rights reserved.
 */

#import "GTMSenTestCase.h"

#import "PLCrashFrameWalker.h"


struct test_stack_args {
    pthread_mutex_t lock;
    pthread_cond_t cond;
};

/* A thread that exists just to give us a stack to iterate */
static void *test_stack_thr (void *arg) {
    struct test_stack_args *args = arg;
    
    /* Acquire the lock and inform our caller that we're active */
    pthread_mutex_lock(&args->lock);
    pthread_cond_signal(&args->cond);
    
    /* Wait for a shut down request, and then drop the acquired lock immediately */
    pthread_cond_wait(&args->cond, &args->lock);
    pthread_mutex_unlock(&args->lock);
    
    return NULL;
}


static void spawn_test_thread (pthread_t *thr, struct test_stack_args *args) {
    /* Initialize the args */
    pthread_mutex_init(&args->lock, NULL);
    pthread_cond_init(&args->cond, NULL);
    
    /* Lock and start the thread */
    pthread_mutex_lock(&args->lock);
    pthread_create(thr, NULL, test_stack_thr, args);
    pthread_cond_wait(&args->cond, &args->lock);
    pthread_mutex_unlock(&args->lock);
}

static void stop_test_thread (pthread_t *thr, struct test_stack_args *args) {
    /* Signal the thread to exit */
    pthread_mutex_lock(&args->lock);
    pthread_cond_signal(&args->cond);
    pthread_mutex_unlock(&args->lock);
    
    /* Wait for exit */
    pthread_join(*thr, NULL);
}

@interface PLCrashFrameWalkerTests : SenTestCase {
@private
    pthread_t _test_thr;
    struct test_stack_args _thr_args;
}
@end

@implementation PLCrashFrameWalkerTests
    
- (void) setUp {
    spawn_test_thread(&_test_thr, &_thr_args);
}

- (void) tearDown {
    stop_test_thread(&_test_thr, &_thr_args);
}

/* test plframe_valid_stackaddr() */
- (void) testValidStackAddress {

}


/* test plframe_cursor_init() */
- (void) testInitFrame {
    plframe_cursor_t cursor;

    /* Initialize the cursor */
    STAssertEquals(PLFRAME_ESUCCESS, plframe_cursor_thread_init(&cursor, pthread_mach_thread_np(_test_thr)), @"Initialization failed");

    /* Try fetching the first frame */
    plframe_error_t ferr = plframe_cursor_next(&cursor);
    STAssertEquals(PLFRAME_ESUCCESS, ferr, @"Next failed: %s", plframe_strerror(ferr));
}

@end
