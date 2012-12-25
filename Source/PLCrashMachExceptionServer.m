/*
 * Author: Landon Fuller <landonf@bikemonkey.org>
 *
 * Copyright (c) 2012 Plausible Labs Cooperative, Inc.
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

#import "PLCrashMachExceptionServer.h"
#import "PLCrashReporterNSError.h"

#import <pthread.h>
#import <mach/mach.h>
#import <mach/exc.h>

#define EXCEPTION_MASK 

/**
 * @internal
 * Mask of monitored fatal exceptions.
 */
static const exception_mask_t FATAL_EXCEPTION_MASK = EXC_MASK_BAD_ACCESS |
                                                     EXC_MASK_BAD_INSTRUCTION |
                                                     EXC_MASK_ARITHMETIC |
                                                     EXC_MASK_BREAKPOINT;

/**
 * @internal
 *
 * Exception state as returned by task_get_exception_ports(). Up
 * to EXC_TYPES_COUNT entries may be returned. The actual count
 * is provided via exception_handler_state::count. The values
 * stored in the arrays correspond positionally.
 */
struct exception_handler_state {
    /** Number of independent mask/port/behavior/flavor sets
     * (up to EXC_TYPES_COUNT). */
    mach_msg_type_number_t count;

    /** Exception masks. */
    exception_mask_t masks[EXC_TYPES_COUNT];

    /** Exception ports. */
    mach_port_t ports[EXC_TYPES_COUNT];

    /** Exception behaviors. */
    exception_behavior_t behaviors[EXC_TYPES_COUNT];

    /** Exception thread flavors. */
    thread_state_flavor_t flavors[EXC_TYPES_COUNT];
};

/**
 * Exception handler context.
 */
struct exception_server_context {
    /** The target mach task. */
    task_t task;

    /** Registered exception port. */
    mach_port_t server_port;

    /** Lock used to signal waiting initialization thread. */
    pthread_mutex_t server_init_lock;
    
    /** Condition used to signal waiting initialization thread. */
    pthread_cond_t server_init_cond;

    /** Intended to be observed by the waiting initialization thread. Contains
     * the result of setting the task exception handler. */
    kern_return_t server_init_result;

    /** Previously registered handlers. We will forward all exceptions here after
      * internal processing. */
    struct exception_handler_state prev_handler_state;

    /** Registered handler state. */
    struct exception_handler_state handler_state;
};

/***
 * @internal
 * Implements Crash Reporter mach exception handling.
 */
@implementation PLCrashMachExceptionServer

/**
 * Initialize a new Mach exception server. The exception server will
 * not register itself in the handler chain until PLCrashMachExceptionServer::registerHandlerAndReturnError:
 * is called.
 */
- (id) init {
    if ((self = [super init]) == nil)
        return nil;

    return self;
}

/**
 * Background exception server. Handles incoming exception messages and dispatches
 * them to the registered callback.
 *
 * This code must be written to be async-safe once a Mach exception message
 * has been returned, as the state of the process' threads is entirely unknown.
 */
static void *exception_server_thread (void *arg) {
    struct exception_server_context *exc_context = arg;
    __Request__exception_raise_t *request = NULL;
    size_t request_size;
    kern_return_t kr;
    mach_msg_return_t mr;

    /* Set up the exception handler, informing the waiting thread of completion (or failure). */
    pthread_mutex_lock(&exc_context->server_init_lock); {
        kr = task_set_exception_ports(exc_context->task, FATAL_EXCEPTION_MASK, exc_context->server_port, EXCEPTION_DEFAULT, THREAD_STATE_NONE);
        exc_context->server_init_result = kr;

        /* Notify our spawning thread of our initialization */
        pthread_cond_signal(&exc_context->server_init_cond);

        /* Exit on error. If a failure occured, it's now unsafe to access exc_server; the spawning
         * thread may have already deallocated it. */
        if (kr != KERN_SUCCESS) {
            pthread_mutex_unlock(&exc_context->server_init_lock);
            return NULL;
        }
    } pthread_mutex_unlock(&exc_context->server_init_lock);

    /* Initialize the received message with a default size */
    request_size = round_page(sizeof(*request));
    kr = vm_allocate(mach_task_self(), (vm_address_t *) &request, request_size, VM_FLAGS_ANYWHERE);
    if (kr != KERN_SUCCESS) {
        /* Shouldn't happen ... */
        fprintf(stderr, "Unexpected error in vm_allocate(): %x\n", kr);
        return NULL;
    }

    while (true) {
        request->Head.msgh_local_port = exc_context->server_port;
        request->Head.msgh_size = request_size;
        mr = mach_msg(&request->Head,
                      MACH_RCV_MSG | MACH_RCV_LARGE,
                      0,
                      request->Head.msgh_size,
                      exc_context->server_port,
                      MACH_MSG_TIMEOUT_NONE,
                      MACH_PORT_NULL);
        
        if (mr != MACH_MSG_SUCCESS) {
            if (mr == MACH_RCV_TOO_LARGE) {
                /* Determine the new size (before dropping the buffer) */
                request_size = round_page(request->Head.msgh_size);

                /* Drop the old receive buffer */
                vm_deallocate(mach_task_self(), (vm_address_t) request, request_size);

                /* Re-allocate a larger receive buffer */
                kr = vm_allocate(mach_task_self(), (vm_address_t *) &request, request_size, VM_FLAGS_ANYWHERE);
                if (kr != KERN_SUCCESS) {
                    /* Shouldn't happen ... */
                    fprintf(stderr, "Unexpected error in vm_allocate(): %x\n", kr);
                    return NULL;
                }

                continue;
            }
            
            // TODO - handle MACH_RCV_INTERRUPTED and/or thread shutdown

            /* Shouldn't happen ... */
            fprintf(stderr, "Unexpected error in mach_msg(): %x\n", kr);
            break;
        }
        

        // TODO - Handle message
        fprintf(stderr, "Got mach exception message. exc=%x code=%x,%x\n", request->exception, request->code[0], request->code[1]);

        // TODO - Send reply

        break;
    }

    /* Drop the receive buffer */
    if (request != NULL)
        vm_deallocate(mach_task_self(), (vm_address_t) request, request_size);

    return NULL;
}

/**
 * Register the task signal handlers with the provided callback.
 * Should not be called more than once.
 *
 * @note If this method returns NO, the exception handler will remain unmodified.
 * @warning Inserting a new Mach task handlers is not atomic, and no support for
 * synchronization is possible through the Mach APIs; thus, a race condition exists
 * if task exception handlers are concurrently modified (in-process or by an
 * external process). It is possible that a handler set concurrently to this
 * call will not be forwarded exception messages if it is registered between this call's
 * fetching of the previous handler (to which exceptions <em>will</em> be forwarded), and
 * installation of the new handler (which will forward exceptions to the previously
 * fetched handler).
 *
 * @param task The mach task for which the handler should be registered.
 * @param callback Callback called upon receipt of an exception. The callback will execute
 * on the exception server's thread, distinctly from the crashed thread.
 * @param context Context to be passed to the callback. May be NULL.
 * @param outError A pointer to an NSError object variable. If an error occurs, this
 * pointer will contain an error object indicating why the signal handlers could not be
 * registered. If no error occurs, this parameter will be left unmodified. You may specify
 * NULL for this parameter, and no error information will be provided.
 */
- (BOOL) registerHandlerForTask: (task_t) task
                   withCallback: (PLCrashMachExceptionHandlerCallback) callback
                        context: (void *) context
                          error: (NSError **) outError
{
    struct exception_server_context *exc_context = NULL;
    pthread_attr_t attr;
    pthread_t thr;

    exc_context = calloc(1, sizeof(*context));
    exc_context->task = task;
    exc_context->server_port = MACH_PORT_NULL;
    exc_context->server_init_result = KERN_SUCCESS;

    if (pthread_mutex_init(&exc_context->server_init_lock, NULL) != 0) {
        plcrash_populate_posix_error(outError, errno, @"Mutex initialization failed");
        free(exc_context);
        return NO;
    }

    if (pthread_cond_init(&exc_context->server_init_cond, NULL) != 0) {
        plcrash_populate_posix_error(outError, errno, @"Condition initialization failed");
        free(exc_context);
        return NO;
    }

    /* Fetch the current exception ports */
    exc_context->prev_handler_state.count = EXC_TYPES_COUNT;
    kern_return_t kr = task_get_exception_ports(task, FATAL_EXCEPTION_MASK,
                                                exc_context->prev_handler_state.masks,
                                                &exc_context->prev_handler_state.count,
                                                exc_context->prev_handler_state.ports,
                                                exc_context->prev_handler_state.behaviors,
                                                exc_context->prev_handler_state.flavors);
    if (kr != KERN_SUCCESS) {
        plcrash_populate_mach_error(outError, kr, @"Failed to fetch existing task exception ports");
        goto error;
    }
    
    /*
     * Initalize our server's port
     */
    kr = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &exc_context->server_port);
    if (kr != KERN_SUCCESS) {
        plcrash_populate_mach_error(outError, kr, @"Failed to allocate exception server's port");
        goto error;
    }

    kr = mach_port_insert_right(mach_task_self(), exc_context->server_port, exc_context->server_port, MACH_MSG_TYPE_MAKE_SEND);
    if (kr != KERN_SUCCESS) {
        plcrash_populate_mach_error(outError, kr, @"Failed to add send right to exception server's port");
        goto error;
    }
    
    /* Spawn the server thread. The server thread will be responsible for actually setting the task exception
     * ports; this must happen last, as we gaurantee that the new exception ports will be either fully configured,
     * or left unchanged, and pthread_create() may fail. */
    {
        if (pthread_attr_init(&attr) != 0) {
            plcrash_populate_posix_error(outError, errno, @"Failed to initialize pthread_attr");
            goto error;
        }

        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        // TODO - A custom stack should be specified, using high/low guard pages to help prevent overwriting the stack
        // by crashing code.
        // pthread_attr_setstack(&attr, sp, stacksize);
        
        if (pthread_create(&thr, &attr, &exception_server_thread, exc_context) != 0) {
            plcrash_populate_posix_error(outError, errno, @"Failed to create exception server thread");
            pthread_attr_destroy(&attr);
            goto error;
        }
        
        pthread_attr_destroy(&attr);
    }

    pthread_mutex_lock(&exc_context->server_init_lock);
    pthread_cond_wait(&exc_context->server_init_cond, &exc_context->server_init_lock);
    if (exc_context->server_init_result != KERN_SUCCESS) {
        plcrash_populate_mach_error(outError, exc_context->server_init_result, @"Failed to set task exception ports");
        goto error;
    }
    pthread_mutex_unlock(&exc_context->server_init_lock);
    
    return YES;

error:
    if (exc_context != NULL) {
        if (exc_context->server_port != MACH_PORT_NULL)
            mach_port_deallocate(mach_task_self(), exc_context->server_port);

        pthread_cond_destroy(&exc_context->server_init_cond);
        pthread_mutex_destroy(&exc_context->server_init_lock);

        free(exc_context);
    }

    return NO;
}

kern_return_t plcrash_exception_raise (
   mach_port_t exception_port,
   mach_port_t thread,
   mach_port_t task,
   exception_type_t exception,
   exception_data_t code, mach_msg_type_number_t codeCnt)
{
    return KERN_SUCCESS;
}

kern_return_t plcrash_exception_raise_state (
   mach_port_t exception_port,
   exception_type_t exception,
   const exception_data_t code,
   mach_msg_type_number_t codeCnt,
   int *flavor,
   const thread_state_t old_state,
   mach_msg_type_number_t old_stateCnt,
   thread_state_t new_state,
   mach_msg_type_number_t *new_stateCnt)
{
    return KERN_SUCCESS;
}

kern_return_t plcrash_exception_raise_state_identity (
   mach_port_t exception_port,
   mach_port_t thread,
   mach_port_t task,
   exception_type_t exception,
   exception_data_t code,
   mach_msg_type_number_t codeCnt,
   int *flavor,
   thread_state_t old_state,
   mach_msg_type_number_t old_stateCnt,
   thread_state_t new_state,
   mach_msg_type_number_t *new_stateCnt)
{
    return KERN_SUCCESS;
}

@end
