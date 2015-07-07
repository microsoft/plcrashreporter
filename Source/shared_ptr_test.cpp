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

#include "shared_ptr.hpp"
#include "weak_ptr.hpp"

#include "PLCrashCatchTest.hpp"

PLCR_CPP_BEGIN_ASYNC_NS

TEST_CASE("Shared References") {
    AsyncAllocator *allocator;
    REQUIRE(AsyncAllocator::Create(&allocator, PAGE_SIZE) == PLCRASH_ESUCCESS);
    
    WHEN("Wrapping primitive types") {
        auto ptr = make_shared<int>(allocator, 42);

        THEN("Primitive value should be dereferenceable by value and pointer-to-value") {
            REQUIRE(*ptr == 42);
            REQUIRE(*(ptr.get()) == 42);
        }
    }
    
    WHEN("Wrapping C++ types") {
        /* Verify all dereferencing styles work on classes */
        class ExampleClass {
        public:
            int val;
            ExampleClass (int v) : val(v) { }
        };

        auto ptr = make_shared<ExampleClass>(allocator, 42);
        THEN("Object value should be dereferenceable by value and pointer-to-value") {
            REQUIRE((*ptr).val == 42);
            REQUIRE((ptr.get())->val == 42);
            REQUIRE(ptr->val == 42);
        }
        
        THEN("Reference should not be empty") {
            REQUIRE(!ptr.isEmpty());
        }
    }
    
    WHEN("Operating on an empty shared reference") {
        shared_ptr<int> ptr;

        THEN("Retain count should be 0") {
            REQUIRE(ptr.referenceCount() == 0);
        }
        
        THEN("Reference should be empty") {
            REQUIRE(ptr.isEmpty());
        }
        
        THEN("Retain count should be 1 once a value is set") {
            ptr = make_shared<int>(allocator, 42);
            REQUIRE(ptr.referenceCount() == 1);
        }
        
        THEN("Returned reference should be null") {
            REQUIRE(ptr.get() == nullptr);
        }
    }
    
    WHEN("Copying shared references") {
        auto ptr = make_shared<int>(allocator, 42);
        
        THEN("Retain count should start at 1") {
            REQUIRE(ptr.referenceCount() == 1);
        }

        THEN("Retain count should increase after copy construction") {
            auto copiedPtr = ptr;
            REQUIRE(copiedPtr.referenceCount() == 2);
            REQUIRE(ptr.referenceCount() == 2);
        }
        
        THEN("Retain count should increase after copy assignment") {
            shared_ptr<int> copiedPtr;
            copiedPtr = ptr;
            REQUIRE(copiedPtr.referenceCount() == 2);
            REQUIRE(ptr.referenceCount() == 2);
        }
    }
    
    WHEN("Moving shared references") {
        THEN("Retain count should remain constant after move construction") {
            shared_ptr<int> ptr(atl::move(make_shared<int>(allocator, 42)));
            REQUIRE(ptr.referenceCount() == 1);
        }

        THEN("Retain count should remain constant after move assignment") {
            shared_ptr<int> ptr;
            ptr = make_shared<int>(allocator, 42);
            REQUIRE(ptr.referenceCount() == 1);
        }
    }
    
    WHEN("Clearing shared references") {
        auto ptr = make_shared<int>(allocator, 42);
        THEN("The reference should be empty after clear()") {
            REQUIRE(ptr.referenceCount() == 1);
            REQUIRE(!ptr.isEmpty());

            ptr.clear();

            REQUIRE(ptr.referenceCount() == 0);
            REQUIRE(ptr.isEmpty());
        }
    }
    
    WHEN("Constructing from weak references") {
        THEN("empty weak references should return empty strong references") {
            weak_ptr<int> wptr;
            shared_ptr<int> sptr = wptr;
            REQUIRE(sptr.isEmpty());
        }

        THEN("live weak references should return live strong references") {
            auto sptr1 = make_shared<int>(allocator, 42);
            weak_ptr<int> wptr(sptr1);

            /* New strong pointer must be non-empty */
            auto sptr2 = shared_ptr<int>(wptr);
            REQUIRE(!sptr2.isEmpty());
            
            /* Reference count should now be 2 */
            REQUIRE(sptr2.referenceCount() == 2);
            REQUIRE(sptr1.referenceCount() == sptr2.referenceCount());
            
            /* Value dereferencing should actually work */
            REQUIRE(*sptr2 == 42);
            REQUIRE(*sptr2 == *sptr1);
        }

        THEN("dead weak references should return empty strong references") {
            weak_ptr<int> wptr;
            
            /* Use a block scope to control the strong ptr's lifetime. */
            {
                auto sptr = make_shared<int>(allocator, 42);
                wptr = sptr;
                REQUIRE(sptr.referenceCount() == 1);
            }
            
            /* Weak pointer should now point to a dead value. */
            REQUIRE(wptr.referenceCount() == 0);

            /* The strong pointer should detect the zero refcount and return an empty strong pointer */
            shared_ptr<int> sptr = wptr;
            REQUIRE(sptr.isEmpty());
        }
    }
    
    WHEN("Destroying a shared reference") {
        class ExampleClass {
        public:
            ExampleClass (size_t *destCount) : _destCount(destCount) {}
            ~ExampleClass () {
                *_destCount = *_destCount + 1;
            }
        private:
            size_t *_destCount;
        };
        
        THEN("The managed object's destructor should be executed once (and only once)") {
            /* Tracks the number of times ExampleClass' destructor was invoked. */
            size_t destCount = 0;
            
            /* Use a block scope to control the ptr's lifetime. */
            {
                auto ptr = make_shared<ExampleClass>(allocator, &destCount);
                REQUIRE(destCount == 0);
                REQUIRE(ptr.referenceCount() == 1);
            }

            /** Assert that the destructor was called once and only once */
            REQUIRE(destCount == 1);
        }
    }
    
    WHEN("Comparing empty shared references") {
        shared_ptr<int> ref;

        THEN("empty references should have a bool value of false") {
            REQUIRE(!ref);
        }
        
        THEN("empty references should be equal") {
            shared_ptr<int> ref2;
            REQUIRE(ref == ref2);
        }
        
        THEN("empty references should compare as equal to nullptr") {
            class ExampleClass {};
            REQUIRE(ref == nullptr);
            REQUIRE(nullptr == ref); // Compare both forms of the overload

        }
        
        THEN("empty references should not be equal to non-empty references") {
            auto nonempty = make_shared<int>(allocator, 42);
            REQUIRE(ref != nonempty);
        }
    }
    
    WHEN("Comparing non-empty shared references") {
        auto ref = make_shared<int>(allocator, 42);

        THEN("non-empty references should have a bool value of true") {
            REQUIRE(ref);
        }
        
        THEN("non-empty references should be equal if their pointer values are equal") {
            /* Point at different values, will not be equal */
            auto refNEQ = make_shared<int>(allocator, 42);
            REQUIRE(refNEQ != ref);

            /* Point at the same value, will be equal */
            auto refEQ = ref;
            REQUIRE(ref == refEQ);
        }

        THEN("non-empty references should not compare as equal to nullptr") {
            REQUIRE(ref != nullptr);
            REQUIRE(nullptr != ref); // Compare both forms of the overload
        }
        
        THEN("non-empty references should not be equal to empty references") {
            shared_ptr<int> empty;
            REQUIRE(ref != empty);
        }
    }
    
    /* Clean up */
    delete allocator;
}

PLCR_CPP_END_ASYNC_NS
