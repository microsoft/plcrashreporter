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

#ifndef PLCRASH_ASYNC_WEAK_PTR_H
#define PLCRASH_ASYNC_WEAK_PTR_H

#include "PLCrashMacros.h"

#include "Reference.hpp"

/**
 * @internal
 * @ingroup plcrash_async
 *
 * @{
 */

PLCR_CPP_BEGIN_ASYNC_NS
    
/* Forward-declare shared_ptr */
template <typename T> class shared_ptr;

/**
 * The weak_ptr class holds a weak reference to a reference counted object.
 *
 * A weak_ptr may be atomically converted to a strong shared_ptr, if a strong reference to the managed object can be acquired.
 *
 * @tparam T The referenced object type.
 *
 * @par Thread Safety
 *
 * Reference counting is fully thread-safe, and multiple weak_ptr instances may reference the same object. A single
 * weak_ptr instance, however, must not be concurrently mutated -- or accessed during mutation -- without
 * external synchronization.
 */
template <typename T> class weak_ptr : public AsyncAllocatable {
private:
    /* Mark shared_ptr as a friend to allow access to our backing _impl. */
    friend class shared_ptr<T>;

public:
    /**
     * Construct an empty weak reference.
     */
    constexpr weak_ptr () noexcept { }
    
    /**
     * Construct a weak reference to the object managed by @a ref.
     *
     * @param ref A strong reference to an object of type `T`.
     */
    weak_ptr (const shared_ptr<T> &ref) : _impl(ref._impl.sharedValue(), true) {}

    /**
     * Atomically acquire and return a strong reference. If a strong reference to the reference counted object can not
     * be acquired, the returned strong reference will be empty.
     */
    shared_ptr<T> strongReference () {
        return shared_ptr<T>(*this);
    }

    /**
     * Return the current strong reference count for the managed object, or 0 if there is no managed object. The
     * reference count should be equivalent to the number of shared_ptr instances managing the current object.
     *
     * @warning The reference count is not a reliable indicator of object lifetime or liveness, and should not be used
     * outside of testing or debugging.
     */
    size_t referenceCount (void) const {
        return _impl.referenceCount();
    }

    /** Release ownership of the managed object, if any. Upon return, the weak reference will be empty. */
    inline void clear () {
        _impl.clear();
    }
    
private:
    /** Reference implementation */
    refcount::Reference<T, refcount::InlineReferencedValue<T>, refcount::WeakReferenceType<T, refcount::InlineReferencedValue<T>>> _impl;
};

PLCR_CPP_END_ASYNC_NS

/**
 * @}
 */

#endif /* PLCRASH_ASYNC_WEAK_PTR_H */
