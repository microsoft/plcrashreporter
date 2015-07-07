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

#ifndef PLCRASH_ASYNC_SHARED_PTR_H
#define PLCRASH_ASYNC_SHARED_PTR_H

#include "AsyncAllocatable.hpp"
#include "AsyncAllocator.hpp"

#include "Reference.hpp"
#include "PLCrashMacros.h"

#include "async_stl.hpp"

/**
 * @internal
 * @ingroup plcrash_async
 *
 * @{
 */

PLCR_CPP_BEGIN_ASYNC_NS

/* Forward-declare weak_ptr */
template <typename T> class weak_ptr;

/**
 * The shared_ptr class holds a strong reference to a reference counted object.
 *
 * The object is destroyed when the last remaining shared_ptr is destroyed, or otherwise releases its ownership
 * of the shared object.
 *
 * @tparam T The referenced object type.
 *
 * @par Thread Safety
 *
 * Reference counting is fully thread-safe, and multiple shared_ptr instances may reference the same shared object. A
 * single shared_ptr instance, however, must not be concurrently mutated -- or accessed during mutation -- without
 * external synchronization.
 */
template <typename T> class shared_ptr : public AsyncAllocatable {
private:
    /**
     * Construct a new shared reference, assuming ownership of the @a value. The reference count of @a value will not
     * be incremented.
     *
     * @param value The referenced value for the newly constructed shared reference, or NULL if the new shared
     * reference should be empty.
     */
    shared_ptr (refcount::InlineReferencedValue<T> *value) : _impl(value, false) { }
    
    /* Mark weak_ptr as a friend to allow access to our backing _impl. */
    friend class weak_ptr<T>;

    /* Grant access to internal elements of other template instantiations */
    template <typename U> friend class shared_ptr;
    
    /* Type tag used to enable is_convertible constructor */
    struct convertible_tag {};

public:
    /**
     * Construct an empty shared reference.
     */
    constexpr shared_ptr () noexcept { }
    
    /**
     * Construct a shared reference by atomically acquiring a strong reference to an existing shared_ptr<>T>
     */
    shared_ptr (const shared_ptr<T> &other) = default;

    /**
     * Construct a shared reference by atomically moving a strong reference from an existing shared_ptr<>T>
     */
    shared_ptr (shared_ptr<T> &&other) = default;
    
    /**
     * Copy assignment.
     */
    shared_ptr &operator= (const shared_ptr<T> &other) = default;
    
    /**
     * Move assignment.
     */
    shared_ptr &operator= (shared_ptr<T> &&other) = default;

    /**
     * Construct a shared reference by atomically acquiring a strong reference to an existing weak_ptr<T>.
     *
     * If a strong reference to the reference counted object can not be acquired, the new strong reference will be empty.
     */
    shared_ptr (const weak_ptr<T> &weak) {
        /* Fetch the backing shared value. If the value is NULL, the weakptr is empty. */
        auto v = weak._impl.sharedValue();
        if (v == nullptr) {
            return;
        }

        /* Atomically increment the reference count using compare-and-swap to ensure that it hasn't changed since we
         * fetched its value. We know that the sharedValue itself won't be deallocated, as the weak_ptr reference
         * we've been given will keep it alive. */
        int64_t refCount;
        do {
            /* If the strong reference count is already zero, the object is dead, and we're done. */
            refCount = v->refs;
            if (refCount == 0)
                return;
            
            /* Loop until we successfully swap in the new value. */
        } while (!__sync_bool_compare_and_swap(&v->refs, refCount, refCount+1));

        /* If we managed to get here, we now own a strong reference; insert it into our backing Reference instance,
         * letting it adopt our acquired strong reference. */
        _impl.put(v, false);
    }

    /**
     * Construct a new shared reference wrapping a newly constructed instance of @a T, using @a args as the parameter
     * list for the constructor of @a T.
     *
     * @param allocator The allocator to be used to instantiate the new instance.
     * @param args The arguments with which an instance of @a T will be constructed.
     */
    template <class ...Args> static shared_ptr<T> make_shared(AsyncAllocator *allocator, Args&&... args) {
        refcount::InlineReferencedValue<T> *s = new (allocator) refcount::InlineReferencedValue<T>(atl::forward<Args>(args)...);
        return shared_ptr<T>(s);
    }

    /**
     * Return true if this reference is empty.
     *
     * @note An empty reference is equivalent to a reference to a null pointer.
     */
    inline bool isEmpty (void) const {
        return _impl.isEmpty();
    }
    
    /**
     * Return true if this reference is *not* empty.
     *
     * @note An empty reference is equivalent to a reference to a null pointer.
     *
     * This is suitable for use in conditional expressions, for example:
     * @code
     if (ptr) {
        ...
     }
     @endcode
     */
    inline explicit operator bool () const {
        return !isEmpty();
    }

    /**
     * Return the current strong reference count for the managed object, or 0 if there is no managed object. The
     * reference count should be equal to the number of shared_ptr instances referencing the managed object.
     *
     * @warning The reference count is not a reliable indicator of object lifetime or liveness, and should not be used
     * outside of testing or debugging.
     */
    inline size_t referenceCount (void) const {
        return _impl.referenceCount();
    }

    inline size_t use_count (void) const {
        return referenceCount();
    }
    
    /** Returns a pointer to the managed object. */
    inline T* get (void) const {
        return _impl.get();
    }
    
    /** Dereferences the pointer to the managed object */
    inline T& operator *() const {
        return *_impl.get();
    }
    
    /** Dereferences the pointer to the managed object */
    inline T* operator ->() const {
        return _impl.get();
    }
    
    /** Release ownership of the managed object, if any. Upon return, the shared reference will be empty. */
    inline void clear () {
        _impl.clear();
    }

private:
    /** Reference implementation */
    refcount::Reference<T, refcount::InlineReferencedValue<T>, refcount::StrongReferenceType<T, refcount::InlineReferencedValue<T>>> _impl;
};

/**
 * Construct an object of type @a T and wraps it in a reference counting shared_ptr, using @a args
 * as the parameter list for the constructor of @a T.
 *
 * @param allocator The allocator to be used to instantiate the new instance.
 * @param args The arguments with which an instance of @a T will be constructed.
 *
 * @tparam T The object type.
 */
template <class T, class ...Args> shared_ptr<T> make_shared (AsyncAllocator *allocator, Args&&... args) {
    return shared_ptr<T>::make_shared(allocator, atl::forward<Args>(args)...);
};

/**
 * Compare two shared references, returning true if the referenced objects are equal.
 */
template <class T, class U> inline bool operator== (const shared_ptr<T> &ptr1, const shared_ptr<U> &ptr2) {
    return ptr1.get() == ptr2.get();
}

/**
 * Compare two shared references, returning true if the referenced objects are not equal.
 */
template <class T, class U> inline bool operator!= (const shared_ptr<T> &ptr1, const shared_ptr<U> &ptr2) {
    return !(ptr1 == ptr2);
}
    
/**
 * Compare a shared reference to a pointer, returning true if the referenced objects are equal.
 */
template <class T> inline bool operator== (const shared_ptr<T> &ptr, atl::nullptr_t) {
    return ptr.isEmpty();
}

/**
 * Compare a shared reference to a pointer, returning true if the referenced objects are equal.
 */
template <class U> inline bool operator== (atl::nullptr_t, const shared_ptr<U> &ptr) {
    return ptr.isEmpty();
}

/**
 * Compare a shared reference to a pointer, returning true if the referenced objects are not equal.
 */
template <class T> inline bool operator!= (const shared_ptr<T> &ptr, atl::nullptr_t) {
    return !ptr.isEmpty();
}

/**
 * Compare a shared reference to a pointer, returning true if the referenced objects are not equal.
 */
template <class U> inline bool operator!= (atl::nullptr_t, const shared_ptr<U> &ptr) {
    return !ptr.isEmpty();
}

PLCR_CPP_END_ASYNC_NS

/**
 * @}
 */

#endif /* PLCRASH_ASYNC_SHARED_PTR_H */
