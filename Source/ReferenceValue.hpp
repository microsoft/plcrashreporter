/*
 * Copyright (c) 2014 - 2015 Landon Fuller <landon@landonf.org>.
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

#include "PLCrashMacros.h"
#include "PLCrashAsync.h"

#include "AsyncAllocatable.hpp"
#include "async_stl.hpp"

#ifndef PLCRASH_ASYNC_REFERENCE_VALUE_H
#define PLCRASH_ASYNC_REFERENCE_VALUE_H

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
 * An abstract reference counted value. The lifetime of this object -- and the destruction of its
 * backing object of type `T` -- is entirely defined and managed by a corresponding ReferenceType instance.
 *
 * @tparam T The type held by this referenced value.
 */
template <typename T> class ReferenceValue : public AsyncAllocatable {
public:
    /**
     * Construct an empty referenced value with a reference count of 1.
     */
    ReferenceValue() : refs(1), weakRefs(1) {
        /* 
         * Note the initial value of weakRefs -- there is a single implicit
         * weak reference held by -all- strong references to the object; when all strong references are released,
         * this weak reference is also released, allowing deallocation to occur.
         *
         * This approach allows us to avoid any ordering concerns around strong and weak reference behavior, at the cost
         * of one additional atomic operation upon discarding the last strorng reference.
         */
    }

    /** Destroy the referenced value. */
    ~ReferenceValue() {
        PLCF_ASSERT(refs == 0);
    }
    
    /* Alignment Note: These two values should align the concrete subclass' storage at either 8 or 16 bytes,
     * depending on the target platform's size_t width. This should give us optimal layout on most platforms. */
    
    /** This value's strong reference count; if this hits zero, the backing value should be destructed and/or deallocated. */
    volatile size_t refs;

    /** This value's weak reference count; if this hits zero, this containing ReferenceValue should be deallocated. */
    volatile size_t weakRefs;
};

/**
 * @internal
 * An inline-allocated reference-counted value.
 *
 * @tparam T The type held by this referenced value.
 */
template <typename T> class InlineReferencedValue : public ReferenceValue<T> {
public:
    /** Destroy the referenced value. */
    ~InlineReferencedValue () {}

    /**
     * Construct a new inline-allocated value.
     *
     * @param args The arguments with which an instance of @a T will be constructed.
     * @tparam Args The @a T constructor's arguments types.
     */
    template <class ...Args> InlineReferencedValue (Args&&... args) : _value(atl::forward<Args>(args)...) { }

    /**
     * Returns a pointer to the managed object.
     */
    inline T* get (void) noexcept {
        return &_value;
    }

    /**
     * Destroy the backing object allocation.
     */
    inline void destroy (void) {
        _value.~T();
    }
    
private:
    /**
     * Inline allocation of the reference counted value.
     *
     * We use a union to disable default destruction of the value; this allows us to perform destruction
     * as soon as the strong reference count hits zero, even if weak references require that this
     * enclosing InlineReferencedValue remain alive.
     */
    union {
        /** The actual value. */
        T _value;
    };
};

} /* namespace refcount */
PLCR_CPP_END_ASYNC_NS

/**
 * @}
 */

#endif /* PLCRASH_ASYNC_REFERENCE_VALUE_H */
