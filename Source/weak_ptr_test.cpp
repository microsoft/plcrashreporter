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

#include "shared_ptr.hpp"
#include "weak_ptr.hpp"

#include "PLCrashCatchTest.hpp"

PLCR_CPP_BEGIN_ASYNC_NS

TEST_CASE("Weak References") {
    AsyncAllocator *allocator;
    REQUIRE(AsyncAllocator::Create(&allocator, PAGE_SIZE) == PLCRASH_ESUCCESS);
    
    WHEN("Operating on an empty shared reference") {
        weak_ptr<int> wptr;
        
        THEN("Retain count should be 0") {
            REQUIRE(wptr.referenceCount() == 0);
        }
        
        THEN("Retain count should be 1 once a value is set") {
            auto strong = make_shared<int>(allocator, 42);
            wptr = strong;
            REQUIRE(wptr.referenceCount() == 1);
        }
        
        THEN("Retain count should be 0 after the strong reference goes out of scope") {
            REQUIRE(wptr.referenceCount() == 0);
            {
                auto strong = make_shared<int>(allocator, 42);
                wptr = strong;
                REQUIRE(wptr.referenceCount() == 1);
            }
            REQUIRE(wptr.referenceCount() == 0);
        }
    }

    WHEN("Constructing from an existing strong reference") {
        THEN("Strong reference count should not increase when constructing the weak reference") {
            auto ptr = make_shared<int>(allocator, 42);
            REQUIRE(ptr.referenceCount() == 1);

            auto wptr = weak_ptr<int>(ptr);
            REQUIRE(ptr.referenceCount() == 1);
            REQUIRE(wptr.referenceCount() == ptr.referenceCount());
        }
        
        THEN("Retain count should be 0 after the strong reference goes out of scope") {
            weak_ptr<int> *wptr = NULL;
            {
                auto ptr = make_shared<int>(allocator, 42);
                wptr = new (allocator) weak_ptr<int>(ptr);
                REQUIRE(ptr.referenceCount() == 1);
            }
            REQUIRE(wptr->referenceCount() == 0);
            delete wptr;
        }
    }
    
    WHEN("Destroying a weak reference") {
        class ExampleClass {
        public:
            ExampleClass (size_t *destCount) : _destCount(destCount) {}
            ~ExampleClass () {
                *_destCount = *_destCount + 1;
            }
        private:
            size_t *_destCount;
        };
        
        THEN("The managed object's destructor should be executed once (and only once) when only a weak reference remains") {
            /* Tracks the number of times ExampleClass' destructor was invoked. */
            size_t destCount = 0;

            /* Use block scopes to control the weak and strong reference's lifetime. */
            {
                weak_ptr<ExampleClass> wptr;
                {
                    auto ptr = make_shared<ExampleClass>(allocator, &destCount);
                    wptr = ptr;
                    REQUIRE(destCount == 0);
                    REQUIRE(ptr.referenceCount() == 1);
                }

                /** Assert that the destructor was called once and only once after the strong reference went out-of-scope. */
                REQUIRE(destCount == 1);
            }

            /** Assert that the destructor was not called again after the weak reference went out-of-scope. */
            REQUIRE(destCount == 1);
        }
    }

    WHEN("Converting to a strong reference") {
        /* The actual conversion behavior is implemented and tested in shared_ptr; this is just a smoke test */
        THEN("a weak reference must a strong reference") {
            auto sptr = make_shared<int>(allocator, 42);
            weak_ptr<int> ref = sptr;

            /* Try creating a strong reference */
            auto sptr2 = ref.strongReference();
            REQUIRE(sptr2.referenceCount() == 2);
            REQUIRE(sptr.referenceCount() == sptr2.referenceCount());
        }
    }
    
    /* Clean up */
    delete allocator;
}

PLCR_CPP_END_ASYNC_NS
