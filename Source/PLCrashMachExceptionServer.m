/*
 * Author: Landon Fuller <landonf@bikemonkey.org>
 *
 * Copyright (c) 2012-2013 Plausible Labs Cooperative, Inc.
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

/*
 * WARNING:
 *
 * I've held off from implementing Mach exception handling due to the fact that the APIs required for a complete
 * implementation are not public on iOS. However, a commercial crash reporter is now shipping with support for Mach
 * exceptions, which implies that either they've received special dispensation to use private APIs / private structures,
 * they've found another way to do it, or they're just using undocumented functionality and hoping for the best.
 *
 * After filing a request with Apple DTS to clarify the issue, they provided the following guidance:
 *    Our engineers have reviewed your request and have determined that this would be best handled as a bug report,
 *    which you have already filed. _There is no documented way of accomplishing this, nor is there a workaround
 *    possible._
 *
 * Emphasis mine. As such, I don't believe it is be possible to support the use of Mach exceptions on iOS
 * without the use of undocumented functionality.
 *
 * Unfortunately, sigaltstack() is broken in later iOS releases, necessitating an alternative fix. Even if it wasn't
 * broken, it only ever supported handling stack overflow on the main thread, and mach exceptions would be a preferrable
 * solution.
 *
 * As such, this file provides a proof-of-concept implementation of Mach exception handling, intended to
 * provide support for Mac OS X using public API, and to ferret out what cannot be implemented on iOS
 * without the use of private API on iOS. Some developers have requested that Mach exceptions be provided as
 * option on iOS, which we may provide in the future.
 *
 * The following issues exist in the iOS implementation:
 *  - The msgh_id values required for an exception reply message are not available from the available
 *    headers and must be hard-coded. This prevents one from safely replying to exception messages, which
 *    means that it is impossible to (correctly) inform the server that an exception has *not* been
 *    handled.
 *
 *    Impact:
 *      This can lead to the process locking up and not dispatching to the host exception handler (eg, Apple's 
 *      crash reporter), depending on the behavior of the kernel exception code.
 *
 *  - The mach_* structure/type variants required by MACH_EXCEPTION_CODES are not publicly defined (on Mac OS X,
 *    these are provided by mach_exc.defs). This prevents one from forwarding exception messages to an existing
 *    handler that was registered with a MACH_EXCEPTION_CODES behavior.
 *    
 *    Impact:
 *      This can break forwarding to any task exception handler that registers itself with MACH_EXCEPTION_CODES.
 *      This is the case with LLDB; it will register a task exception handler with MACH_EXCEPTION_CODES set. Failure
 *      to correctly forward these exceptions will result in the debugger breaking in interesting ways; for example,
 *      changes to the set of dyld-loaded images are detected by setting a breakpoint on the dyld image registration
 *      funtions, and this functionality will break if the exception is not correctly forwarded.
 *
 * Since Mach exception handling is important for a fully functional crash reporter, I've also filed a radar
 * to request that the API be made public:
 *  Radar: rdar://12939497 RFE: Provide mach_exc.defs for iOS
 */

#import "PLCrashMachExceptionServer.h"
#import "PLCrashReporterNSError.h"
#import "PLCrashAsync.h"

#import <pthread.h>
#import <libkern/OSAtomic.h>

#import <mach/mach.h>
#import <mach/exc.h>

/* On Mac OS X, we are free to use the 64-bit mach_* APIs. No headers are provided for these,
 * but the MIG defs are available and may be included directly in the build */
#if !TARGET_OS_IPHONE
#define HANDLE_MACH64_CODES 1
#import "mach_exc.h"
#endif

#if HANDLE_MACH64_CODES
typedef __Request__mach_exception_raise_t PLRequest_exception_raise_t;
typedef __Reply__mach_exception_raise_t PLReply_exception_raise_t;
#else
typedef __Request__exception_raise_t PLRequest_exception_raise_t;
typedef __Reply__exception_raise_t PLReply_exception_raise_t;
#endif

/**
 * @internal
 * Mask of monitored fatal exceptions.
 *
 * @warning Must be kept in sync with exception_to_mask();
 */
static const exception_mask_t FATAL_EXCEPTION_MASK = EXC_MASK_BAD_ACCESS |
                                                     EXC_MASK_BAD_INSTRUCTION |
                                                     EXC_MASK_ARITHMETIC |
                                                     EXC_MASK_BREAKPOINT;

/**
 * @internal
 * Map an exception type to its corresponding mask value.
 *
 * @note This only needs to handle the values listed in FATAL_EXCEPTION_MASK.
 */
static exception_mask_t exception_to_mask (exception_type_t exception) {
#define EXM(n) case EXC_ ## n: return EXC_MASK_ ## n;
    switch (exception) {
        EXM(BAD_ACCESS);
        EXM(BAD_INSTRUCTION);
        EXM(ARITHMETIC);
        EXM(EMULATION);
        EXM(BREAKPOINT);
        EXM(SOFTWARE);
        EXM(SYSCALL);
        EXM(MACH_SYSCALL);
        EXM(RPC_ALERT);
        EXM(CRASH);
    }
#undef EXM

    PLCF_DEBUG("No mapping available from exception type 0x%d to an exception mask", exception);
    return 0;
}

/**
 * @internal
 *
 * Exception state as returned by task_get_exception_ports(). Up
 * to EXC_TYPES_COUNT entries may be returned. The actual count
 * is provided via exception_handler_state::count. The values
 * stored in the arrays correspond positionally.
 */
struct plcrash_exception_handler_state {
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
struct plcrash_exception_server_context {
    /** The target mach task. */
    task_t task;
    
    /** The target mach thread. MACH_PORT_NULL if a
     * task server is to be registered. */
    thread_t thread;

    /** Registered exception port. */
    mach_port_t server_port;
    
    /** The mask of exceptions for which we're registered. */
    exception_mask_t exc_mask;

    /** User callback. */
    PLCrashMachExceptionHandlerCallback callback;

    /** User callback context. */
    void *callback_context;

    /** Lock used to signal waiting initialization thread. */
    pthread_mutex_t lock;
    
    /** Condition used to signal waiting initialization thread. */
    pthread_cond_t server_cond;
    
    /** Intended to be observed by the waiting initialization thread. Informs
     * the waiting thread that initialization has completed . */
    bool server_init_done;

    /** Intended to be observed by the waiting initialization thread. Contains
     * the result of setting the task exception handler. */
    kern_return_t server_init_result;
    
    /**
     * Intended to be set by a controlling termination thread. Informs the mach exception
     * thread that it should de-register itself and then signal completion.
     *
     * This value must be updated atomically and with a memory barrier, as it will be accessed
     * without locking.
     */
    uint32_t server_should_stop;

    /** Intended to be observed by the waiting initialization thread. Informs
     * the waiting thread that shutdown has completed . */
    bool server_stop_done;
    
    /** Intended to be observed by the waiting initialization thread. Contains
     * the result of resetting the task exception handler. */
    kern_return_t server_stop_result;

    /** Previously registered handlers. We will forward all exceptions here after
      * internal processing. */
    struct plcrash_exception_handler_state prev_handler_state;
};

/**
 * @internal
 *
 * Set exception ports defined in @a state for @a task and @a thread.
 *
 * @param task The task for which the exception handler(s) were registered.
 * @param thread If not MACH_PORT_NULL, the thread for which the exception ports should be set.
 * @param required_mask The mask of exception types for which the ports should be set. If @a state does not contain handlers
 * for all types specified in @a required_mask, a port of MACH_PORT_NULL will be set for the given exception type.
 * @param state Exception handler state.
 */
static void set_exception_ports (task_t task, thread_t thread, exception_mask_t required_mask, struct plcrash_exception_handler_state *state) {
    kern_return_t kr;

    /* Set exception ports for all supplied state entries. */
    exception_mask_t remaining_mask = required_mask;
    for (mach_msg_type_number_t i = 0; i < state->count; ++i) {
        if (MACH_PORT_VALID(!state->ports[i]))
            continue;

        if (thread == MACH_PORT_NULL)
            kr = task_set_exception_ports(task, state->masks[i], state->ports[i], state->behaviors[i], state->flavors[i]);
        else
            kr = thread_set_exception_ports(thread, state->masks[i], state->ports[i], state->behaviors[i], state->flavors[i]);

        /* Log errors, but continue to restore any remaining exception ports */
        if (kr == KERN_SUCCESS) {
            /* Mark as completed */
            remaining_mask &= ~state->masks[i];
        } else {
            PLCF_DEBUG("Failed to restore task exception ports (i=%x, m=%x, p=%x, b=%x, f=%x): %x", i, state->masks[i], state->ports[i], state->behaviors[i], state->flavors[i], kr);
        }
    }

    /* Reset any remaining exception types that were not handled above. */
    if (remaining_mask != 0) {
        if (thread == MACH_PORT_NULL)
            kr = task_set_exception_ports(task, remaining_mask, MACH_PORT_NULL, EXCEPTION_STATE_IDENTITY, MACHINE_THREAD_STATE);
        else
            kr = thread_set_exception_ports(thread, remaining_mask, MACH_PORT_NULL, EXCEPTION_STATE_IDENTITY, MACHINE_THREAD_STATE);
    }
}

/**
 * Send a Mach exception reply for the given @a request and return the result.
 *
 * @param request The request to which a reply should be sent.
 * @param retcode The reply return code to supply.
 */
static mach_msg_return_t exception_server_reply (PLRequest_exception_raise_t *request, kern_return_t retcode) {
    PLReply_exception_raise_t reply;

    /* Initialize the reply */
    memset(&reply, 0, sizeof(reply));
    reply.Head.msgh_bits = MACH_MSGH_BITS(MACH_MSGH_BITS_REMOTE(request->Head.msgh_bits), 0);
    reply.Head.msgh_local_port = MACH_PORT_NULL;
    reply.Head.msgh_remote_port = request->Head.msgh_remote_port;
    reply.Head.msgh_size = sizeof(reply);
    reply.NDR = NDR_record;
    reply.RetCode = retcode;
    
    /*
     * Mach uses reply id offsets of 100. This is rather arbitrary, and in theory could be changed
     * in a future iOS release (although, it has stayed constant for nearly 24 years, so it seems unlikely
     * to change now). See the top-level file warning regarding use on iOS.
     *
     * On Mac OS X, the reply_id offset may be considered implicitly defined due to mach_exc.defs and
     * exc.defs being public.
     */
    reply.Head.msgh_id = request->Head.msgh_id + 100;

    /* Dispatch the reply */
    return mach_msg(&reply.Head, MACH_SEND_MSG, reply.Head.msgh_size, 0, MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
}

/**
 * Forward an exception request directly to @a port.
 *
 * @param request The request to forward.
 * @param port The port to which the exception should be forwarded.
 * @param behavior The behavior with which @a port was registered.
 * @param mach_exception_codes If true, whether MACH_EXCEPTION_CODES was specified for @a port's behavior.
 * @param code32 If mach_exception_codes is false, this array must be populated with 32-bit exception codes
 * that may be passed to the next exception server.
 * @param thread_state The thread state of the exception corresponding to @a request.
 * @param thread_state_count The number of thread state entries in @a thread_state.
 * @param thread_state_flavor The thead flavor of @a thread_state.
 * @param forwarded Set to true if the exception was forwarded, false otherwise.
 *
 * @return If KERN_SUCCESS, the exception server at @a port handled the exception, and the failed thread
 * may be resumed. Otherwise, the exception should be considered unhandled, due to either an error
 * with mach messaging, or due to a negative response from the target @a port.
 */
static kern_return_t exception_server_forward_proxy (PLRequest_exception_raise_t *request,
                                                     mach_port_t port,
                                                     exception_behavior_t behavior,
                                                     bool mach_exception_codes,
                                                     exception_data_type_t code32[],
                                                     thread_state_data_t thread_state,
                                                     mach_msg_type_number_t thread_state_count,
                                                     thread_state_flavor_t thread_state_flavor,
                                                     bool *forwarded)
{
    /* This will only be false in the case where we can't handle the requested exception behavior */
    *forwarded = true;

    /* Handle the supported behaviors */
    switch (behavior) {
        case EXCEPTION_DEFAULT:
            if (mach_exception_codes) {
#if HANDLE_MACH64_CODES
                return mach_exception_raise(port, request->thread.name, request->task.name, request->exception, request->code, request->codeCnt);
#endif
            } else {
                return exception_raise(port, request->thread.name, request->task.name, request->exception, code32, request->codeCnt);
            }
            break;
            
        case EXCEPTION_STATE:
            if (mach_exception_codes) {
#if HANDLE_MACH64_CODES
                return mach_exception_raise_state(port, request->exception, request->code, request->codeCnt, &thread_state_flavor, thread_state,
                                                  thread_state_count, thread_state, &thread_state_count);
#endif
            } else {
                return exception_raise_state(port, request->exception, code32, request->codeCnt, &thread_state_flavor, thread_state,
                                             thread_state_count, thread_state, &thread_state_count);                    
            }
            break;
            
        case EXCEPTION_STATE_IDENTITY:
            if (mach_exception_codes) {
#if HANDLE_MACH64_CODES
                return mach_exception_raise_state_identity(port, request->thread.name, request->task.name, request->exception, request->code,
                                                           request->codeCnt, &thread_state_flavor, thread_state, thread_state_count, thread_state, &thread_state_count);
#endif
            } else {
                return exception_raise_state_identity(port, request->thread.name, request->task.name, request->exception, code32,
                                                      request->codeCnt, &thread_state_flavor, thread_state, thread_state_count, thread_state, &thread_state_count);                    
            }
            break;

        default:
            /* Handled below */
            break;
    }

    PLCF_DEBUG("Unsupported exception behavior: 0x%x (MACH_EXCEPTION_CODES=%s)", behavior, mach_exception_codes ? "true" : "false");
    *forwarded = false;
    return KERN_FAILURE;
}

/**
 * Forward an exception request directly to @a port.
 *
 * @param request The request to forward.
 * @param port The port to which the exception should be forwarded.
 * @param behavior The behavior with which @a port was registered.
 * @param mach_exception_codes If true, whether MACH_EXCEPTION_CODES was specified for @a port's behavior.
 * @param code32 If mach_exception_codes is false, this array must be populated with 32-bit exception codes
 * that may be passed to the next exception server.
 * @param thread_state The thread state of the exception corresponding to @a request.
 * @param thread_state_count The number of thread state entries in @a thread_state.
 * @param thread_state_flavor The thead flavor of @a thread_state.
 *
 * @return If KERN_SUCCESS, the exception message was successfully forwarded to @a port. The result is unknown,
 * as any response will be sent directly to the original Mach exception client.
 */
static kern_return_t exception_server_forward_direct (PLRequest_exception_raise_t *request,
                                                      mach_port_t port,
                                                      exception_behavior_t behavior,
                                                      bool mach_exception_codes,
                                                      exception_data_type_t code32[],
                                                      thread_state_data_t thread_state,
                                                      mach_msg_type_number_t thread_state_count,
                                                      thread_state_flavor_t thread_state_flavor)
{
    union {
        __Request__exception_raise_t fwd_default;
        __Request__exception_raise_state_t fwd_state;
        __Request__exception_raise_state_identity_t fwd_state_identity;

#if HANDLE_MACH64_CODES
        __Request__mach_exception_raise_t mach_fwd_default;
        __Request__mach_exception_raise_state_t mach_fwd_state;
        __Request__mach_exception_raise_state_identity_t mach_fwd_state_identity;
#endif
    } reqs;
    
    /* Initialize common message header */
    mach_msg_base_t *fwd = NULL;
    reqs.fwd_default.Head = request->Head;
    reqs.fwd_default.Head.msgh_remote_port = port;


    /* Populate the message as per the requested behavior. */
    switch (behavior) {
        case EXCEPTION_DEFAULT: {
            if (mach_exception_codes) {
#if HANDLE_MACH64_CODES
                __Request__mach_exception_raise_t *msg = &reqs.mach_fwd_default;
                msg->Head.msgh_size = sizeof(*msg);
                
                msg->thread = request->thread;
                msg->task = request->task;
                msg->NDR = NDR_record;
                msg->exception = request->exception;
                msg->codeCnt = request->codeCnt;
                memcpy(msg->code, request->code, sizeof(request->code[0]) * request->codeCnt);
                PLCF_ASSERT(sizeof(msg->code[0]) == sizeof(request->code[0]));

                fwd = (mach_msg_base_t *) msg;
#endif
            } else {
                __Request__exception_raise_t *msg = &reqs.fwd_default;
                msg->Head.msgh_size = sizeof(*msg);
                
                msg->thread = request->thread;
                msg->task = request->task;
                msg->NDR = NDR_record;
                msg->exception = request->exception;
                msg->codeCnt = request->codeCnt;
                memcpy(msg->code, code32, sizeof(code32[0]) * request->codeCnt);
                PLCF_ASSERT(sizeof(msg->code[0]) == sizeof(code32[0]));

                fwd = (mach_msg_base_t *) msg;
            }
            break;
        }
            
        case EXCEPTION_STATE: {
            if (mach_exception_codes) {
#if HANDLE_MACH64_CODES
                __Request__mach_exception_raise_state_t *msg = &reqs.mach_fwd_state;
                msg->Head.msgh_size = sizeof(*msg);
                
                msg->NDR = NDR_record;
                msg->exception = request->exception;
                msg->codeCnt = request->codeCnt;
                memcpy(msg->code, request->code, sizeof(request->code[0]) * request->codeCnt);
                PLCF_ASSERT(sizeof(msg->code[0]) == sizeof(request->code[0]));

                msg->old_stateCnt = thread_state_count;
                memcpy(msg->old_state, thread_state, sizeof(thread_state[0]) * thread_state_count);
                
                fwd = (mach_msg_base_t *) msg;
#endif
            } else {
                __Request__exception_raise_state_t *msg = &reqs.fwd_state;
                msg->Head.msgh_size = sizeof(*msg);
                
                msg->NDR = NDR_record;
                msg->exception = request->exception;
                msg->codeCnt = request->codeCnt;
                memcpy(msg->code, code32, sizeof(code32[0]) * request->codeCnt);
                PLCF_ASSERT(sizeof(msg->code[0]) == sizeof(code32[0]));

                msg->old_stateCnt = thread_state_count;
                memcpy(msg->old_state, thread_state, sizeof(thread_state[0]) * thread_state_count);
                
                fwd = (mach_msg_base_t *) msg;
            }
            break;
        }
            
        case EXCEPTION_STATE_IDENTITY: {
            if (mach_exception_codes) {
#if HANDLE_MACH64_CODES
                __Request__mach_exception_raise_state_identity_t *msg = &reqs.mach_fwd_state_identity;
                msg->Head.msgh_size = sizeof(*msg);
                
                msg->thread = request->thread;
                msg->task = request->task;
                msg->NDR = NDR_record;
                msg->exception = request->exception;

                msg->codeCnt = request->codeCnt;
                memcpy(msg->code, request->code, sizeof(request->code[0]) * request->codeCnt);
                PLCF_ASSERT(sizeof(msg->code[0]) == sizeof(request->code[0]));

                msg->flavor = thread_state_flavor;
                msg->old_stateCnt = thread_state_count;
                memcpy(msg->old_state, thread_state, sizeof(thread_state[0]) * thread_state_count);
                
                fwd = (mach_msg_base_t *) msg;
#endif
            } else {
                __Request__exception_raise_state_identity_t *msg = &reqs.fwd_state_identity;
                msg->Head.msgh_size = sizeof(*msg);
                
                msg->thread = request->thread;
                msg->task = request->task;
                msg->NDR = NDR_record;
                msg->exception = request->exception;

                msg->codeCnt = request->codeCnt;
                memcpy(msg->code, code32, sizeof(code32[0]) * request->codeCnt);
                PLCF_ASSERT(sizeof(msg->code[0]) == sizeof(code32[0]));

                msg->flavor = thread_state_flavor;
                msg->old_stateCnt = thread_state_count;
                memcpy(msg->old_state, thread_state, sizeof(thread_state[0]) * thread_state_count);
                
                fwd = (mach_msg_base_t *) msg;
            }
            break;
        }
            
        default:
            /* Handled below */
            break;
    }
    
    if (fwd == NULL) {
        PLCF_DEBUG("Unsupported exception behavior: 0x%x (MACH_EXCEPTION_CODES=%s)", behavior, mach_exception_codes ? "true" : "false");
        return KERN_FAILURE;
    }

    return mach_msg(&fwd->header, MACH_SEND_MSG, fwd->header.msgh_size, 0, MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
}

/**
 * Forward a Mach exception to the given exception to the first matching handler in @a state, if any.
 *
 * @param request The incoming request that should be forwarded.
 * @param state The set of exception handlers to which the message should be forwarded.
 * @param forward_direct If false, the exception message will be sent with a local mach reply port,
 * and the result will be returned. If true, the exception message will be sent with its original
 * reply port, and the response will be unavailable.
 * @param forwarded Set to true if the exception was forwarded, false otherwise.
 *
 * @return Returns KERN_SUCCESS if the exception was handled by a registered exception server, or an error
 * if the exception was not handled or no handling server was registered.
 *
 * @par In-Process Operation
 *
 * When operating in-process, handling the exception replies internally breaks external debuggers,
 * as they assume it is safe to leave our thread suspended. This results in the target thread never resuming,
 * as our thread never wakes up to reply to the message. If messages are forwarded directly, rather than proxied,
 * then the current process will be removed from the critical path during exception replies. To do so, the
 * @a forward_direct argument should be true.
 *
 * Note, however, that if we do not remove our exception server from the chain before forwarding a request,
 * we will still block the next exception message, as our exception server's thread may not be restarted.
 * Additionally, if threads are suspended due to an exception message that our exception server is not
 * registered for, we will never de-register ourselves, and the same lockup will occur.
 *
 * Additionally, removing this exception handler will inescapably remove this handler, <em>as well as all handlers
 * registered after this handler</em>, as the relationship between handlers is effectively a single-ordered list.
 *
 * The recommened solution is to simply not register a Mach exception handler in the case where a debugger
 * is already attached. If this can not be avoided, then direct forwarding should be used.
 *
 * @TODO We need to be able to determine if an exception can be/will/was handled by a signal handler. Failure
 * to detect such a case will result in spurious reports written for otherwise handled signals. See also:
 * https://bugzilla.xamarin.com/show_bug.cgi?id=4120
 */
static kern_return_t exception_server_forward (PLRequest_exception_raise_t *request,
                                               struct plcrash_exception_handler_state *state,
                                               bool forward_direct,
                                               bool *forwarded)
{
    exception_behavior_t behavior;
    thread_state_flavor_t flavor;
    thread_state_data_t thread_state;
    mach_msg_type_number_t thread_state_count;
    mach_port_t port;
    kern_return_t kr;
    bool mach_exception_codes = false;

    /* Default state of non-forwarded */
    *forwarded = false;

    /* Find a matching handler */
    exception_mask_t fwd_mask = exception_to_mask(request->exception);
    bool found = false;
    for (mach_msg_type_number_t i = 0; i < state->count; i++) {
        if (!MACH_PORT_VALID(state->ports[i]))
            continue;

        if ((state->masks[i] & fwd_mask) == 0)
            continue;

        found = true;
        port = state->ports[i];
        behavior = state->behaviors[i];
        flavor = state->flavors[i];
        break;
    }

    /* No handler found */
    if (!found) {
        return KERN_FAILURE;
    }
    
    /* Clean up behavior */
    if (behavior & MACH_EXCEPTION_CODES)
        mach_exception_codes = true;
    behavior &= ~MACH_EXCEPTION_CODES;

    /* Fetch thread state if required */
    if (behavior != EXCEPTION_DEFAULT) {
        thread_state_count = THREAD_STATE_MAX;
        kr = thread_get_state (request->thread.name, flavor, thread_state, &thread_state_count);
        if (kr != KERN_SUCCESS) {
            PLCF_DEBUG("Failed to fetch thread state for thread, kr=0x%x", kr);
            return kr;
        }
    }

    /* We prefer 64-bit codes; if the user requests 32-bit codes, we need to map them */
#if HANDLE_MACH64_CODES
    exception_data_type_t code32[request->codeCnt];
    for (mach_msg_type_number_t i = 0; i < request->codeCnt; i++) {
        code32[i] = (uint64_t) request->code[i];
    }
#else
    int32_t *code32 = request->code;
#endif

    /* Use the preferred forwarding mechanism */
    if (forward_direct) {
        kr = exception_server_forward_direct(request, port, behavior, mach_exception_codes, code32, thread_state, thread_state_count, flavor);

        if (kr == KERN_SUCCESS)
            *forwarded = true;
    } else {
        kr = exception_server_forward_proxy(request, port, behavior, mach_exception_codes, code32, thread_state, thread_state_count, flavor, forwarded);
    }

    return kr;
}

/**
 * Background exception server. Handles incoming exception messages and dispatches
 * them to the registered callback.
 *
 * This code must be written to be async-safe once a Mach exception message
 * has been returned, as the state of the process' threads is entirely unknown.
 */
static void *exception_server_thread (void *arg) {
    struct plcrash_exception_server_context *exc_context = arg;
    PLRequest_exception_raise_t *request = NULL;
    size_t request_size;
    kern_return_t kr;
    mach_msg_return_t mr;

    /* Atomically swap in our up the exception handler, also informing the waiting thread of
     * completion (or failure). */
    pthread_mutex_lock(&exc_context->lock); {
        exc_context->prev_handler_state.count = EXC_TYPES_COUNT;

#if HANDLE_MACH64_CODES
        exception_behavior_t behavior = EXCEPTION_DEFAULT | MACH_EXCEPTION_CODES;
#else
        exception_behavior_t behavior = EXCEPTION_DEFAULT;
#endif
        
        if (exc_context->thread == MACH_PORT_NULL) {
            kr = task_swap_exception_ports(exc_context->task,
                                           exc_context->exc_mask,
                                           exc_context->server_port,
                                           behavior,
                                           THREAD_STATE_NONE,
                                           exc_context->prev_handler_state.masks,
                                           &exc_context->prev_handler_state.count,
                                           exc_context->prev_handler_state.ports,
                                           exc_context->prev_handler_state.behaviors,
                                           exc_context->prev_handler_state.flavors);
        } else {
            kr = thread_swap_exception_ports(exc_context->thread,
                                             exc_context->exc_mask,
                                             exc_context->server_port,
                                             behavior,
                                             THREAD_STATE_NONE,
                                             exc_context->prev_handler_state.masks,
                                             &exc_context->prev_handler_state.count,
                                             exc_context->prev_handler_state.ports,
                                             exc_context->prev_handler_state.behaviors,
                                             exc_context->prev_handler_state.flavors);
        }
        exc_context->server_init_result = kr;
        exc_context->server_init_done = true;

        /* Notify our spawning thread of our initialization */
        pthread_cond_signal(&exc_context->server_cond);

        /* Exit on error. If a failure occured, it's now unsafe to access exc_server; the spawning
         * thread may have already deallocated it. */
        if (kr != KERN_SUCCESS) {
            pthread_mutex_unlock(&exc_context->lock);
            return NULL;
        }
    } pthread_mutex_unlock(&exc_context->lock);

    /* Initialize the received message with a default size */
    request_size = round_page(sizeof(*request));
    kr = vm_allocate(mach_task_self(), (vm_address_t *) &request, request_size, VM_FLAGS_ANYWHERE);
    if (kr != KERN_SUCCESS) {
        /* Shouldn't happen ... */
        fprintf(stderr, "Unexpected error in vm_allocate(): %x\n", kr);
        return NULL;
    }

    /* Wait for an exception message */
    while (true) {
        /* Initialize our request message */
        request->Head.msgh_local_port = exc_context->server_port;
        request->Head.msgh_size = request_size;
        mr = mach_msg(&request->Head,
                      MACH_RCV_MSG | MACH_RCV_LARGE,
                      0,
                      request->Head.msgh_size,
                      exc_context->server_port,
                      MACH_MSG_TIMEOUT_NONE,
                      MACH_PORT_NULL);

        /* Handle recoverable errors */
        if (mr != MACH_MSG_SUCCESS && mr == MACH_RCV_TOO_LARGE) {
            /* Determine the new size (before dropping the buffer) */
            request_size = round_page(request->Head.msgh_size);

            /* Drop the old receive buffer */
            vm_deallocate(mach_task_self(), (vm_address_t) request, request_size);

            /* Re-allocate a larger receive buffer */
            kr = vm_allocate(mach_task_self(), (vm_address_t *) &request, request_size, VM_FLAGS_ANYWHERE);
            if (kr != KERN_SUCCESS) {
                /* Shouldn't happen ... */
                fprintf(stderr, "Unexpected error in vm_allocate(): 0x%x\n", kr);
                return NULL;
            }

            continue;

        /* Handle fatal errors */
        } else if (mr != MACH_MSG_SUCCESS) {
            /* Shouldn't happen ... */
            PLCF_DEBUG("Unexpected error in mach_msg(): 0x%x. Restoring exception ports and stopping server thread.", mr);

            /* Restore exception ports; we won't be around to handle future exceptions */
            set_exception_ports(exc_context->task, exc_context->thread, exc_context->exc_mask, &exc_context->prev_handler_state);

            break;

        /* Success! */
        } else {
            /* Detect 'short' messages. */
            if (request->Head.msgh_size < sizeof(*request)) {
                /* We intentionally do not acquire a lock here. It is possible that we've been woken
                 * spuriously with the process in an unknown state, in which case we must not call
                 * out to non-async-safe functions */
                if (exc_context->server_should_stop) {
                    /* Restore exception ports; we won't be around to handle future exceptions */
                    // TODO - This is non-atomic; should we cycle through a mach_msg() call to clear
                    // exception messages that may have been enqueued?
                    set_exception_ports(exc_context->task, exc_context->thread, exc_context->exc_mask, &exc_context->prev_handler_state);

                    /* Inform the requesting thread of completion */
                    pthread_mutex_lock(&exc_context->lock); {
                        // TODO - Report errors resetting the exception ports?
                        exc_context->server_stop_result = KERN_SUCCESS;
                        exc_context->server_stop_done = true;

                        pthread_cond_signal(&exc_context->server_cond);
                    } pthread_mutex_unlock(&exc_context->lock);

                    /* Ensure a quick death if we access exc_context after termination  */
                    exc_context = NULL;

                    /* Trigger cleanup */
                    break;
                }
            }

            /*
             * Detect exceptions from tasks other than our target task. This should only be possible if
             * a crash occurs after a fork(), but before our pthread_atfork() handler de-registers 
             * our exception handler.
             *
             * The exception still needs to be forwarded to any previous exception handlers, but we will
             * not call our callback handler.
             *
             * TODO: Implement pthread_atfork() cleanup handler.
             */
            bool is_monitored_task = true;
            if (request->task.name != exc_context->task) {
                PLCF_DEBUG("Mach exception message received from unexpected task.");
                is_monitored_task = false;
            }

            /* Restore exception ports; we don't want to double-fault if our exception handler crashes */
            // TODO - We should register a thread-specific double-fault handler to specifically handle
            // this state by executing a "safe mode" logging path.
            set_exception_ports(exc_context->task, exc_context->thread, exc_context->exc_mask, &exc_context->prev_handler_state);
            
            /* Map 32-bit codes to 64-bit types. */
#if !HANDLE_MACH64_CODES
            mach_exception_data_type_t code64[request->codeCnt];
            for (mach_msg_type_number_t i = 0; i < request->codeCnt; i++) {
                code64[i] = (uint32_t) request->code[i];
            }
#else
            mach_exception_data_type_t *code64 = request->code;
#endif

            /* Call our handler. */
            kern_return_t exc_result = KERN_FAILURE;
            bool exception_handled;

            exception_handled = exc_context->callback(request->task.name,
                                                     request->thread.name,
                                                     request->exception,
                                                     code64,
                                                     request->codeCnt,
                                                     false, /* TODO: double fault handling */
                                                     exc_context->callback_context);
            
            if (exception_handled) {
                /* Mark as handled */
                exc_result = KERN_SUCCESS;
            } else {
                /* If not exception was not handled internally (ie, the thread was not corrected and resumed), then
                 * forward the exception. */
                kern_return_t forward_result;
                bool forwarded;

                // TODO - allow selecting between forwarding mechanisms
                forward_result = exception_server_forward(request, &exc_context->prev_handler_state, true, &forwarded);
                if (forwarded)
                    exc_result = forward_result;
            }

            /*
             * Reply to the message.
             */
            mr = exception_server_reply(request, exc_result);
            if (mr != MACH_MSG_SUCCESS)
                PLCF_DEBUG("Unexpected failure replying to Mach exception message: 0x%x", mr);
        }
    }

    /* Drop the receive buffer */
    if (request != NULL)
        vm_deallocate(mach_task_self(), (vm_address_t) request, request_size);

    return NULL;
}


/***
 * @internal
 * Implements Crash Reporter mach exception handling.
 */
@implementation PLCrashMachExceptionServer

/**
 * Initialize a new Mach exception server. The exception server will
 * not register itself in the handler chain until
 * PLCrashMachExceptionServer::registerHandlerForTask:withCallback:context:error is called.
 */
- (id) init {
    if ((self = [super init]) == nil)
        return nil;

    return self;
}

/**
 * Register the task signal handlers with the provided callback.
 *
 * @note If this method returns NO, the exception handler will remain unmodified.
 * @note Inserting the Mach task handler is performed atomically, and multiple handlers
 * may be initialized concurrently.
 *
 * @param task The mach task for which the handler should be registered.
 * @param thread The mach thread for which the handler should be registered. If MACH_PORT_NULL,
 * a task-global handler will be registered.
 * @param callback Callback called upon receipt of an exception. The callback will execute
 * on the exception server's thread, distinctly from the crashed thread.
 * @param context Context to be passed to the callback. May be NULL.
 * @param outError A pointer to an NSError object variable. If an error occurs, this
 * pointer will contain an error object indicating why the exception handler could not be
 * registered. If no error occurs, this parameter will be left unmodified. You may specify
 * NULL for this parameter, and no error information will be provided.
 */
- (BOOL) registerHandlerForTask: (task_t) task
                         thread: (thread_t) thread
                   withCallback: (PLCrashMachExceptionHandlerCallback) callback
                        context: (void *) context
                          error: (NSError **) outError
{
    pthread_attr_t attr;
    pthread_t thr;
    kern_return_t kr;
    
    NSAssert(_serverContext == NULL, @"Register called twice!");

    /* Initialize the bare context. */
    _serverContext = calloc(1, sizeof(*_serverContext));
    _serverContext->task = task;
    _serverContext->thread = thread;
    _serverContext->exc_mask = FATAL_EXCEPTION_MASK;
    _serverContext->server_port = MACH_PORT_NULL;
    _serverContext->server_init_result = KERN_SUCCESS;
    _serverContext->callback = callback;
    _serverContext->callback_context = context;

    if (pthread_mutex_init(&_serverContext->lock, NULL) != 0) {
        plcrash_populate_posix_error(outError, errno, @"Mutex initialization failed");

        free(_serverContext);
        _serverContext = NULL;

        return NO;
    }

    if (pthread_cond_init(&_serverContext->server_cond, NULL) != 0) {
        plcrash_populate_posix_error(outError, errno, @"Condition initialization failed");

        free(_serverContext);
        _serverContext = NULL;

        return NO;
    }

    /*
     * Initalize our server's port
     */
    kr = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &_serverContext->server_port);
    if (kr != KERN_SUCCESS) {
        plcrash_populate_mach_error(outError, kr, @"Failed to allocate exception server's port");
        goto error;
    }

    kr = mach_port_insert_right(mach_task_self(), _serverContext->server_port, _serverContext->server_port, MACH_MSG_TYPE_MAKE_SEND);
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
        
        if (pthread_create(&thr, &attr, &exception_server_thread, _serverContext) != 0) {
            plcrash_populate_posix_error(outError, errno, @"Failed to create exception server thread");
            pthread_attr_destroy(&attr);
            goto error;
        }
        
        pthread_attr_destroy(&attr);
    }

    pthread_mutex_lock(&_serverContext->lock);
    
    while (!_serverContext->server_init_done)
        pthread_cond_wait(&_serverContext->server_cond, &_serverContext->lock);

    if (_serverContext->server_init_result != KERN_SUCCESS) {
        plcrash_populate_mach_error(outError, _serverContext->server_init_result, @"Failed to set task exception ports");
        goto error;
    }
    pthread_mutex_unlock(&_serverContext->lock);

    return YES;

error:
    if (_serverContext != NULL) {
        if (_serverContext->server_port != MACH_PORT_NULL)
            mach_port_deallocate(mach_task_self(), _serverContext->server_port);

        pthread_cond_destroy(&_serverContext->server_cond);
        pthread_mutex_destroy(&_serverContext->lock);

        free(_serverContext);
        _serverContext = NULL;
    }

    return NO;
}

/**
 * De-register the mach exception handler.
 
 * @param outError A pointer to an NSError object variable. If an error occurs, this
 * pointer will contain an error object indicating why the signal handlers could not be
 * registered. If no error occurs, this parameter will be left unmodified. You may specify
 * NULL for this parameter, and no error information will be provided.
 *
 * @warning Removing the exception handler for a currently executing process may lead to
 * undefined behavior, and should be avoided in production code. Once registered, a handler
 * should remain valid until the termination of a process. If multiple Mach task handlers have
 * been registered, it is not possible for the receiver to correctly restore the appropriate
 * exception ports, as the ports saved at the time of registration will no longer correspond to
 * top-level registered handler. Additionally, a reference to the receiver's Mach exception
 * ports may have been saved by another exception handler; there is no way to inform said
 * handler of the termination of the receiver, or direct it to use a different target when
 * forwarding exception messages. As such, once registered, an exception server should continue
 * to run for the lifetime of the target thread or process.
 *
 * @warning Removing the Mach task handler is not currently performed atomically; if an
 * exception message is received during deregistration, the exception may be lost.
 */
- (BOOL) deregisterHandlerAndReturnError: (NSError **) outError {
    mach_msg_return_t mr;
    BOOL result = YES;

    NSAssert(_serverContext != NULL, @"No handler registered!");

    /* Mark the server for termination */
    OSAtomicCompareAndSwap32Barrier(0, 1, (int32_t *) &_serverContext->server_should_stop);

    /* Wake up the waiting server */
    mach_msg_header_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, MACH_MSG_TYPE_MAKE_SEND_ONCE);
    msg.msgh_local_port = MACH_PORT_NULL;
    msg.msgh_remote_port = _serverContext->server_port;
    msg.msgh_size = sizeof(msg);
    msg.msgh_id = 0;

    mr = mach_msg(&msg, MACH_SEND_MSG, msg.msgh_size, 0, MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);

    if (mr != MACH_MSG_SUCCESS) {
        /* It's defined by the mach headers as safe to treat a mach_msg_return_t as a kern_return_t */
        plcrash_populate_mach_error(outError, mr, @"Failed to send termination message to background thread");
        return NO;
    }

    /* Wait for completion */
    pthread_mutex_lock(&_serverContext->lock);
    while (!_serverContext->server_stop_done) {
        pthread_cond_wait(&_serverContext->server_cond, &_serverContext->lock);
    }
    pthread_mutex_unlock(&_serverContext->lock);

    if (_serverContext->server_stop_result != KERN_SUCCESS) {
        result = NO;
        plcrash_populate_mach_error(outError, _serverContext->server_stop_result, @"Failed to reset mach exception handlers");
    }

    /* Once we've been signaled by the background thread, it will no longer access exc_context */
    free(_serverContext);
    _serverContext = NULL;

    return result;
}

@end
