/*
 * Author: Landon Fuller <landonf@plausible.coop>
 *
 * Copyright (c) 2008-2013 Plausible Labs Cooperative, Inc.
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

#import "GTMSenTestCase.h"

#include "Iterator.hpp"

using namespace plcrash::async;

@interface IteratorTests : SenTestCase @end

@implementation IteratorTests

/**
 * A class that represents a range of integers.
 */
class IntRange {
private:
    int _start;
    int _end;
    
public:
    /* A simple iterator; walks a consequence range of integers */
    class iterator : public Iterator<int> {
    private:
        int _value;
        
    public:
        iterator (int value) {
            _value = value;
        };
        
        // from Iterator (prefix increment)
        iterator operator++ () {
            _value++;
            return *this;
        }
        
        // from Iterator
        int operator* () const {
            return _value;
        }
        
        // from Iterator
        int operator-> () const {
            return _value;
        }
        
        // from Iterator
        bool operator!= (const iterator &other) const {
            return other._value != _value;
        };
    };
    
    /**
     * Construct a new IntRange instance.
     *
     * @param start The (inclusive) start of the range.
     * @param end The (exclusive) end of the range.
     */
    IntRange (int start, int end) {
        _start = start;
        _end = end;
    };
    
    /** Return an iterator pointing to the first element in the range */
    iterator begin () {
        return iterator(_start);
    };

    /** Returns a iterator pointing to the past-the-end element in the range. */
    iterator end () {
        return iterator(_end);
    };
    
};

/**
 * Test postfix/prefix increment operators. This mainly validates that our IntRange is correctly
 * implemented, which in theory provides some minimal validation that our underlying Iterator
 * specification is correct.
 */
- (void) testIncrement {
    IntRange range = IntRange(0, 2);
    auto iter = range.begin();

    /* The iterator should start with a value of 0 */
    STAssertEquals(*iter, 0, @"Incorrect initial value");

    /* Prefix increment should return the *new* value, *not* the initial value */
    auto prefixed = *(++iter);
    STAssertEquals(prefixed, 1, @"Incorrect prefix increment result value");
    
    /* The iterator should the same value returned by prefix increment */
    STAssertEquals(*iter, prefixed, @"Incorrect postfix iteration state");
}

/**
 * Test handling of C++11 ranged for loops.
 */
- (void) testRangedFor {
    const int count = 10;

    IntRange range = IntRange(0, count);
    
    int values[count];
    memset(values, 0, sizeof(values));

    /* Try iterating the range */
    int vpos = 0;
    for (int i : range) {
        STAssertTrue(vpos < count, @"Overran the expected range");

        values[vpos] = i;
        vpos++;
    }
    
    /* Verify the results */
    for (int i = 0; i < count; i++) {
        STAssertEquals(values[i], i, @"Incorrect value was iterated");
    }
}

@end
