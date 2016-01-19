/*
 * Author: Landon Fuller <landonf@plausible.coop>
 *
 * Copyright (c) 2015 Plausible Labs Cooperative, Inc.
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

#ifndef PLCRASH_ASYNC_SPINLOCK_H
#define PLCRASH_ASYNC_SPINLOCK_H

#include "PLCrashAsync.h"
#include "PLCrashMacros.h"

/**
 * @internal
 * @ingroup plcrash_async
 *
 * @{
 */

PLCR_CPP_BEGIN_ASYNC_NS

/**
 * An async-safe spinlock implementation.
 *
 * @todo Investigate use of C11 atomics here.
 */
class SpinLock {
public:
    /**
     * Construct a new spin lock.
     */
    SpinLock () : _mtx(0) {
        /* Make sure everyone sees an initialized _mtx value. */
        __sync_synchronize();
    }
    
    /* Copying or moving a lock is unsupported. */
    SpinLock (const SpinLock &) = delete;
    SpinLock (SpinLock &&) = delete;
    
    SpinLock &operator= (const SpinLock &) = delete;
    SpinLock &operator= (SpinLock &&) = delete;
    
    /** Attempt to acquire the lock, returning true on success, or false if the lock could not be acquired. */
    inline bool tryLock () {
        return __sync_bool_compare_and_swap(&_mtx, 0, 1);
    }

    /** Acquire the lock. */
    inline void lock () {
        while (!tryLock());
    }

    /** Release the lock. */
    inline void unlock () {
        bool released = __sync_bool_compare_and_swap(&_mtx, 1, 0);
        
        /* If this isn't true, the lock wasn't actually held by the caller. */
        if (!released) {
            PLCF_DEBUG("Released lock that was not held by the caller!");
            PLCF_ASSERT(released);
        }
    }

private:
    /** Lock state; a value of '1' is locked, '0' is unlocked. */
    volatile uint32_t _mtx = 0;
};

PLCR_CPP_END_ASYNC_NS

/**
 * @}
 */

#endif /* PLCRASH_ASYNC_SPINLOCK_H */
