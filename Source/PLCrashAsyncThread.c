/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008-2013 Plausible Labs Cooperative, Inc.
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

#include "PLCrashAsyncThread.h"

/**
 * @internal
 * @ingroup plcrash_async
 * @defgroup plcrash_async_thread Thread State Handling
 *
 * An async-safe and architecture neutral API for introspecting and permuting thread state.
 * @{
 */

/**
 * Initialize the @a thread_state using the provided context.
 *
 * @param thread_state The thread state to be initialized.
 * @param uap The context to use for cursor initialization.
 */
void plcrash_async_tstate_ucontext_init (plcrash_async_tstate_t *thread_state, ucontext_t *uap) {
    /* Copy in the ucontext's thread state */
#if defined(PLCRASH_ASYNC_THREAD_ARM_SUPPORT)
    /* Sanity check. This should never be false */
    PLCF_ASSERT(sizeof(uap->uc_mcontext->__ss) == sizeof(thread_state->arm_state.thread));
    plcrash_async_memcpy(&thread_state->arm_state.thread, &uap->uc_mcontext->__ss, sizeof(thread_state->arm_state.thread));
    
#elif defined(PLCRASH_ASYNC_THREAD_X86_SUPPORT) && defined(__LP64__)
    thread_state->x86_state.thread.tsh.count = x86_THREAD_STATE64_COUNT;
    thread_state->x86_state.thread.tsh.flavor = x86_THREAD_STATE64;
    plcrash_async_memcpy(&thread_state->x86_state.thread.uts.ts64, &uap->uc_mcontext->__ss, sizeof(thread_state->x86_state.thread.uts.ts64));
    
    thread_state->x86_state.exception.esh.count = x86_EXCEPTION_STATE64_COUNT;
    thread_state->x86_state.exception.esh.flavor = x86_EXCEPTION_STATE64;
    plcrash_async_memcpy(&thread_state->x86_state.exception.ues.es64, &uap->uc_mcontext->__es, sizeof(thread_state->x86_state.exception.ues.es64));

#elif defined(PLCRASH_ASYNC_THREAD_X86_SUPPORT)
    thread_state->x86_state.thread.tsh.count = x86_THREAD_STATE32_COUNT;
    thread_state->x86_state.thread.tsh.flavor = x86_THREAD_STATE32;
    plcrash_async_memcpy(&thread_state->x86_state.thread.uts.ts32, &uap->uc_mcontext->__ss, sizeof(thread_state->x86_state.thread.uts.ts32));
    
    thread_state->x86_state.exception.esh.count = x86_EXCEPTION_STATE32_COUNT;
    thread_state->x86_state.exception.esh.flavor = x86_EXCEPTION_STATE32;
    plcrash_async_memcpy(&thread_state->x86_state.exception.ues.es32, &uap->uc_mcontext->__es, sizeof(thread_state->x86_state.exception.ues.es32));
#else
#error Add platform support
#endif
}

/**
 * Initialize the @a thread_state using thread state fetched from the given mach @a thread. If the thread is not
 * suspended, the fetched state may be inconsistent.
 *
 * @param thread_state The thread state to be initialized.
 * @param thread The thread from which to fetch thread state.
 *
 * @return Returns PLFRAME_ESUCCESS on success, or standard plframe_error_t code if an error occurs.
 */
plcrash_error_t plcrash_async_tstate_mach_thread_init (plcrash_async_tstate_t *thread_state, thread_t thread) {
    mach_msg_type_number_t state_count;
    kern_return_t kr;
    
#ifdef PLCRASH_ASYNC_THREAD_ARM_SUPPORT
    /* Fetch the thread state */
    state_count = ARM_THREAD_STATE_COUNT;
    kr = thread_get_state(thread, ARM_THREAD_STATE, (thread_state_t) &thread_state->arm_state.thread, &state_count);
    if (kr != KERN_SUCCESS) {
        PLCF_DEBUG("Fetch of x86 thread state failed with Mach error: %d", kr);
        return PLCRASH_EINTERNAL;
    }
#elif defined(PLCRASH_ASYNC_THREAD_X86_SUPPORT)
    /* Fetch the thread state */
    state_count = x86_THREAD_STATE_COUNT;
    kr = thread_get_state(thread, x86_THREAD_STATE, (thread_state_t) &thread_state->x86_state.thread, &state_count);
    if (kr != KERN_SUCCESS) {
        PLCF_DEBUG("Fetch of x86 thread state failed with Mach error: %d", kr);
        return PLCRASH_EINTERNAL;
    }
    
    /* Fetch the exception state */
    state_count = x86_EXCEPTION_STATE_COUNT;
    kr = thread_get_state(thread, x86_EXCEPTION_STATE, (thread_state_t) &thread_state->x86_state.exception, &state_count);
    if (kr != KERN_SUCCESS) {
        PLCF_DEBUG("Fetch of x86 exception state failed with Mach error: %d", kr);
        return PLCRASH_EINTERNAL;
    }
#else
#error Add platform support
#endif
    
    return PLCRASH_ESUCCESS;
}

/**
 * @}
 */