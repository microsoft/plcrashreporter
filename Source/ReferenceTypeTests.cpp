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

#include "PLCrashCatchTest.hpp"

#include "ReferenceValue.hpp"
#include "ReferenceType.hpp"

using namespace plcrash::async;
using namespace plcrash::async::refcount;

PLCR_CPP_BEGIN_ASYNC_NS

/** A test value class that increments and decrements a counter on construction and destruction, respectively */
class RefTestTarget {
public:
    /* Increment _destCount on construction */
    RefTestTarget (ssize_t *destCount) : _destCount(destCount) { *_destCount = *_destCount + 1; }
    
    /* Copy and move constructors that delegate to our standard _destCount-modifying constructor */
    RefTestTarget (RefTestTarget &other) : RefTestTarget(other._destCount) { }
    RefTestTarget (RefTestTarget &&other) : RefTestTarget(other._destCount) { }
    
    /* Decrement _destCount on destruction */
    ~RefTestTarget () { *_destCount = *_destCount - 1; }
private:
    ssize_t *_destCount;
};


TEST_CASE("StrongReferenceType") {
    AsyncAllocator *allocator;
    REQUIRE(AsyncAllocator::Create(&allocator, PAGE_SIZE) == PLCRASH_ESUCCESS);

    /** The reference type under test */
    StrongReferenceType<RefTestTarget, InlineReferencedValue<RefTestTarget>> refType;
    ssize_t count = 0;

    /* Allocate a test value we'll use with the reftype */
    auto value = new (allocator) InlineReferencedValue<RefTestTarget>(&count);
    REQUIRE(value->refs == (size_t)1);
    REQUIRE(value->weakRefs == (size_t)1);
    REQUIRE(count == 1);

    WHEN("Retaining") {
        THEN("The strong reference count should be incremented") {
            refType.retain(value);
            REQUIRE(value->refs == (size_t)2);
            REQUIRE(value->weakRefs == (size_t)1);
            refType.release(value);
        }
    }
    
    WHEN("Releasing") {
        THEN("The strong reference count should be decremented") {
            refType.retain(value);
            REQUIRE(value->refs == (size_t)2);
            REQUIRE(value->weakRefs == (size_t)1);
            refType.release(value);
            REQUIRE(value->refs == (size_t)1);
            REQUIRE(value->weakRefs == (size_t)1);
        }
        
        THEN("The underlying object should be deconstructed when the strong reference count hits 0") {
            REQUIRE(count == 1);
            refType.release(value);
            REQUIRE(count == 0);

            /* Mark our value as already deallocated, avoiding a double-free below */
            value = nullptr;
        }
    }

    /* Clean up our allocated test value, if not already done in a test. This should result in a balanced
     * construction/destruction count. */
    if (value != nullptr) {
        REQUIRE(value->refs == (size_t)1);
        refType.release(value);
    }
    REQUIRE(count == 0);
    
    /* Clean up our allocator */
    delete allocator;
}

TEST_CASE("WeakReferenceType") {
    AsyncAllocator *allocator;
    REQUIRE(AsyncAllocator::Create(&allocator, PAGE_SIZE) == PLCRASH_ESUCCESS);

    /** The reference type under test */
    WeakReferenceType<RefTestTarget, InlineReferencedValue<RefTestTarget>> refType;
    
    /** A strong reference type that we'll use to test weak/strong interaction */
    StrongReferenceType<RefTestTarget, InlineReferencedValue<RefTestTarget>> strongRefType;
    
    ssize_t count = 0;
    
    /* Allocate a test value we'll use with the reftype */
    auto value = new (allocator) InlineReferencedValue<RefTestTarget>(&count);
    REQUIRE(value->refs == (size_t)1);
    REQUIRE(value->weakRefs == (size_t)1);
    REQUIRE(count == 1);
    
    WHEN("Retaining") {
        THEN("The weak reference count should be incremented") {
            refType.retain(value);
            REQUIRE(value->refs == (size_t)1);
            REQUIRE(value->weakRefs == (size_t)2);
            refType.release(value);
        }
    }
    
    WHEN("Releasing") {
        THEN("The weak reference count should be decremented") {
            refType.retain(value);
            REQUIRE(value->refs == (size_t)1);
            REQUIRE(value->weakRefs == (size_t)2);
            refType.release(value);
            REQUIRE(value->refs == (size_t)1);
            REQUIRE(value->weakRefs == (size_t)1);
        }

        THEN("With a weak reference held, the value reference should not be deallocated when the strong reference count hits 0") {
            /* Create a new weak reference. */
            refType.retain(value);
            REQUIRE(value->weakRefs == (size_t)2);

            /* Release the only strong reference. */
            REQUIRE(count == 1);
            strongRefType.release(value);

            /* The underlying object should have had its destructor called. */
            REQUIRE(count == 0);

            /* But our value should still exist with a live weak reference count. */
            REQUIRE(value->refs == (size_t)0);
            REQUIRE(value->weakRefs == (size_t)1);
            
            /* Drop our weak reference, triggering actual deallocation. */
            refType.release(value);

            /* Mark our value as already deallocated, avoiding a double-free below. */
            value = nullptr;
        }
    }

    /* Clean up our allocated test value, if not already done in a test. This should result in a balanced
     * construction/destruction count. */
    if (value != nullptr) {
        REQUIRE(value->refs == (size_t)1);
        REQUIRE(value->weakRefs == (size_t)1);
        strongRefType.release(value);
    }
    REQUIRE(count == 0);
    
    /* Clean up our allocator */
    delete allocator;
}

PLCR_CPP_END_ASYNC_NS

