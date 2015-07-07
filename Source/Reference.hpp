/*
 * Copyright (c) 2014 - 2015 Plausible Labs Cooperative, Inc.
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

#ifndef PLCRASH_ASYNC_REFERENCE_H
#define PLCRASH_ASYNC_REFERENCE_H

#include "PLCrashMacros.h"
#include "ReferenceValue.hpp"
#include "ReferenceType.hpp"

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
 * Manages access to (and reference counting of) of a backing ReferenceValue instance.
 *
 * This class serves as a shared implementation used by both shared_ptr and weak_ptr.
 *
 * @tparam T The referenced object type.
 * @tparam ReferencedValue The referenced object container.
 * @tparam ReferenceType The reference type implementation (strong, weak, etc).
 *
 * @par Thread Safety
 *
 * Reference counting is fully thread-safe, and multiple Reference instances may reference the same object. A single
 * Reference instance, however, must not be concurrently mutated -- or accessed during mutation -- without
 * external synchronization.
 */
template <typename T, class ReferencedValue, class ReferenceType> class Reference {
public:
    /**
     * Construct a new Reference, optionally incrementing the reference count of @a value.
     *
     * @param value The value for the newly constructed reference, or NULL.
     * @param acquireReference If true, the reference count of a (non-NULL) @a value will be incremented.
     */
    Reference(ReferencedValue *value, bool acquireReference) : _v(nullptr) {
        put(value, acquireReference);
    }
    
    /**
     * Construct an empty reference.
     */
    constexpr Reference() noexcept : _v(nullptr) { }
    
    /**
     * Deallocate this reference and decrement the managed object's reference count. If the reference count is zero,
     * the referenced object is destroyed.
     */
    ~Reference() {
        release();
    }

    /** Returns a pointer to the managed object. */
    inline T* get (void) const {
        if (_v == nullptr) {
            return nullptr;
        } else {
            return _v->get();
        }
    }
    
    /** Release ownership of the managed object, if any. Upon return, the reference will be empty. */
    inline void clear () {
        release();
    }
    
    /**
     * Return the current retain count for the managed object, or 0 if there is no managed object. The retain count should
     * be equivalent to the number of shared_ptr instances managing the current object.
     *
     * Note that the reference count is unreliable, and should not be used.
     */
    size_t referenceCount (void) const {
        if (this->_v == NULL) {
            return 0;
        } else {
            return _v->refs;
        }
    }
    
    /**
     * Move constructor; the move destination adopts ownership of the value referenced by @a ref.
     *
     * @param ref The shared pointer to be moved.
     */
    Reference(Reference &&ref) noexcept : Reference(ref.sharedValue(), false) {
        /* Now that we've moved ownership, forcibly discard the previous owner's reference. */
        ref.abandon();
    }
    
    /**
     * Move assignment operator; the move destination adopts ownership of the value referenced by @a ref.
     *
     * @param ref The shared pointer to be moved.
     */
    Reference & operator= (Reference &&ref) noexcept {
        /* Assume ownership of the shared value */
        put(ref.sharedValue(), false);
        
        /* Forcibly discard the previous owner's reference. */
        ref.abandon();
        
        return *this;
    }
    
    /**
     * Copy constructor; the copy destination retains an additional reference to the value referenced by @a ref.
     *
     * @param ref The shared pointer to be copied.
     */
    Reference(const Reference &ref) noexcept : Reference(ref.sharedValue(), true) { }
    
    /**
     * Copy assignment operator; the copy destination retains an additional reference to the value referenced by @a ref.
     *
     * @param ref The shared pointer to be copied.
     */
    Reference &operator= (const Reference &ref) noexcept {
        /* Claim a reference */
        put(ref.sharedValue(), true);
        return *this;
    }


    /**
     * Return a borrowed reference to the underlying referenced value.
     */
    ReferencedValue *sharedValue (void) const noexcept {
        return _v;
    }
    
    /**
     * Return true if this object holds a reference to an empty (null) pointer.
     */
    bool isEmpty (void) const {
        if (_v == nullptr) {
            return true;
        } else {
            return false;
        }
    }
    
    /**
     * Set the internal reference counted value to @a target, releasing the current value, if any.
     *
     * @param newValue The value to be stored, or nullptr.
     * @param acquireReference If true, and @a target is not nullptr, the reference count of @a target will be
     * incremented. If false, a reference must have already been acquired (eg, by initializing `newValue` with a
     * reference count of 1).
     */
    void put (ReferencedValue *newValue, bool acquireReference) {
        /* Already referencing this value? */
        if (_v == newValue)
            return;
        
        /* Drop our existing shared value (if any). */
        release();
        
        /* Save the new value and, if requested and non-NULL, increment its reference count */
        _v = newValue;
        if (acquireReference && _v != nullptr) {
            retain();
        }
    }

private:
    /**
     * Release our reference to the shared value.
     */
    void release (void) noexcept {
        if (_v != nullptr) {
            _refType.release(_v);
            _v = nullptr;
        }
    }
    
    /**
     * Retain a reference to the shared value.
     */
    void retain (void) noexcept {
        if (_v != nullptr) {
            _refType.retain(_v);
        }
    }
    
    /**
     * Abandon the internal reference counted value; this is primarily used when implementing ownership move behavior,
     * as the move target will assume ownership of our current reference.
     */
    inline void abandon (void) {
        _v = nullptr;
    }

    /** The reference counted value. */
    ReferencedValue *_v;

    /** The reference type */
    ReferenceType _refType;
};

} /* namespace refcount */
PLCR_CPP_END_ASYNC_NS

/**
 * @}
 */

#endif /* PLCRASH_ASYNC_REFERENCE_H */
