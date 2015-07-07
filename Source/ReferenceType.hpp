/*
 * Copyright (c) 2014 Plausible Labs Cooperative, Inc.
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

#ifndef PLCRASH_ASYNC_REFERENCE_TYPE_H
#define PLCRASH_ASYNC_REFERENCE_TYPE_H

#include "AsyncAllocatable.hpp"

#include "ReferenceValue.hpp"
#include "PLCrashMacros.h"

/**
 * @internal
 * @ingroup plcrash_async
 *
 * @{
 */

PLCR_CPP_BEGIN_ASYNC_NS
namespace refcount {
    
/**
 * @internal
 *
 * A weak reference type. Will adjust a ReferenceValue's weak reference count, and deallocate
 * the ReferenceValue instance upon hitting a weak reference count of zero.
 *
 * @tparam T The type of object managed by the target ReferenceValue.
 * @tparam ReferencedValue The referenced object container.
 */
template <typename T, typename ReferencedValue> class WeakReferenceType : public AsyncAllocatable {
public:
    /**
     * Atomically increment the internal reference count.
     */
    inline void retain (ReferencedValue *sharedValue) noexcept {
        __sync_fetch_and_add(&sharedValue->weakRefs, 1);
    }
    
    /**
     * Atomically decrement the internal reference count.
     */
    inline void release (ReferencedValue *sharedValue) noexcept {
        /*
         * Atomically decrement the weak reference. If this hits zero, then all strong and weak references
         * are zero (an implicit weak reference is held by all strong references), and the sharedValue itself
         * can be deallocated.
         */
        if (__sync_fetch_and_sub(&sharedValue->weakRefs, 1) == 1) {
            delete sharedValue;
        }
    }
};

/**
 * @internal
 *
 * A strong reference type. Will adjust a ReferenceValue's strong reference count, destroying the
 * backing object when the strong reference count hits zero.
 *
 * @tparam T The type of object managed by the target ReferenceValue.
 * @tparam ReferencedValue The referenced object container.
 */
template <typename T, typename ReferencedValue> class StrongReferenceType : public AsyncAllocatable {
public:
    /** Atomically increment the internal reference count. */
    inline void retain (ReferencedValue *sharedValue) noexcept {
        __sync_fetch_and_add(&sharedValue->refs, 1);
    }
    
    /**
     * Atomically decrement the internal reference count, deallocating this object if the
     * reference count has hit zero.
     */
    inline void release (ReferencedValue *sharedValue) noexcept {
        /* If the strong reference count hits zero, the value itself can be deconstructed/deallocated, and our
         * implicit weak reference may also be discarded. */
        if (__sync_fetch_and_sub(&sharedValue->refs, 1) == 1) {
            sharedValue->destroy();
            
            /* Discard the implicit weak reference held by all strong references. If this is the last weak reference,
             * this will deallocate the sharedValue itself. */
            _weak.release(sharedValue);
        }
    }

private:
    /** Weak reference handler */
    WeakReferenceType<T, ReferencedValue> _weak;
};
    
} /* namespace refcount */
PLCR_CPP_END_ASYNC_NS

/**
 * @}
 */

#endif /* PLCRASH_ASYNC_REFERENCE_TYPE_H */
