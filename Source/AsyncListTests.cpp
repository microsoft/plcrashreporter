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

#include "AsyncList.hpp"

PLCR_CPP_BEGIN_ASYNC_NS

TEST_CASE("AsyncList") {
    AsyncAllocator *allocator;
    REQUIRE(AsyncAllocator::Create(&allocator, PAGE_SIZE) == PLCRASH_ESUCCESS);

    WHEN("constructing an empty list") {
        List<int> l;
        
        REQUIRE(l.size() == 0);
        REQUIRE(l.isEmpty());
        
        THEN("the tail should also be empty") {
            REQUIRE(l.tail().isEmpty());
        }
    }
    
    WHEN("constructing a single-element list") {
        List<int> l(allocator, 42);
        
        REQUIRE(l.size() == 1);
        REQUIRE(!l.isEmpty());
        
        THEN("the tail should be empty") {
            REQUIRE(l.tail().isEmpty());
        }
    }
    
    WHEN("prepending an element") {
        WHEN("the list is empty") {
            List<int> l;
            auto prepended = l.prepend(allocator, 42);
            
            REQUIRE(prepended.head() == 42);
            REQUIRE(prepended.size() == 1);
            REQUIRE(prepended.tail().isEmpty());
        }
        
        WHEN("the list is not empty") {
            List<int> l(allocator, 42);
            auto prepended = l.prepend(allocator, 84);
            
            REQUIRE(prepended.head() == 84);
            REQUIRE(prepended.size() == 2);
            REQUIRE(!prepended.tail().isEmpty());
            REQUIRE(prepended.tail().head() == 42);
        }
    }
    
    
    /* Clean up */
    delete allocator;
}

PLCR_CPP_END_ASYNC_NS
