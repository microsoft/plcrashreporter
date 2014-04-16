/*
 * C++ port of monadic Result class from Plausible Scala Commons.
 *
 * Copyright (c) 2013-2014 Plausible Labs Cooperative, Inc.
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

#ifndef PLCRASH_ASYNC_EITHER_H
#define PLCRASH_ASYNC_EITHER_H

#include "PLCrashAsync.h"
#include "Iterator.hpp"
#include <stdint.h>

namespace plcrash { namespace async {

/**
 * Represents a result value of one of two possible types (a disjoint union).
 *
 * @tparam E The error type.
 * @tparam T The success type.
 */
template <class E, class T> class Result {
private:
    /**
     * If true, this is a successful result. If false,
     * it's an error result.
     */
    bool _isSuccess;
    
    /**
     * Holds either the error or success value, depending
     * on the value of @a _success;
     */
    union {
        /** The error value, if this is an error instance. */
        E error;
        
        /** The success value, if this is a success instance. */
        T success;
    } _value;
    
    /** Iteration position */
    enum IterPos {
        /** Iteration is active */
        ITER_RUN = 0,
        
        /** Iteration has terminated */
        ITER_END = 1
    };

public:
    /**
     * Construct a successful result.
     *
     * @param success The successful result value.
     */
    Result (T success) {
        _isSuccess = true;
        _value.success = success;
    }

    /**
     * Construct a failure result.
     *
     * @param error The error result value.
     */
    Result (E error) {
        _isSuccess = false;
        _value.error = error;
    }


    /*
     * Iteration support
     */


    /**
     * Iterates once over a successful Result<E, T> value.
     */
    class iterator : public Iterator<T> {
    friend class Result;
    private:
        /** The backing result */
        Result _result;
        
        /** If 0, iteration hasn't started. If 1, this is the first and only valid element. If 2, iteration has completed. */
        IterPos _pos;
        
        /**
         * Construct an iterator over @a result with the given @a position.
         *
         * @param result The result instance to be iterated over.
         * @param completed True if this is the end iterator instance.
         */
        iterator (const Result result, IterPos position) : _result(result), _pos(position) { }

    public:
        // from Iterator
        iterator operator++ () {
            /* Advance to our next iteration position. There are only two; the running state
             * where we've iterated to the single available value, and the end state, where 
             * we've iterated "past" the end of the available value. */
            switch (_pos) {
                case Result::ITER_RUN:   _pos = Result::ITER_END;
                case Result::ITER_END:   break;
            }
            return *this;
        }
        
        // from Iterator
        T operator* () const {
            /* Fetching the value from a terminal iterator is a programming error */
            PLCF_ASSERT(_pos == Result::ITER_RUN);
            return _result._value.success;
        }
        
        // from Iterator
        T operator-> () const {
            /* Fetching the value from a terminal iterator is a programming error */
            PLCF_ASSERT(_pos == Result::ITER_RUN);
            return _result._value.success;
        }
        
        // from Iterator
        bool operator!= (const iterator &other) const {
            return (other._pos != _pos);
        };
    };
    
    /**
     * Return an iterator pointing to the successful value. If this result failed, the
     * iterator will terminate immediately.
     */
    iterator begin () {
        /* On success, we return a working iterator; on failure, we return an already 
         * completed iterator */
        if (_isSuccess) {
            return iterator(*this, Result::ITER_RUN);
        } else {
            return end();
        }
    };
    
    /**
     * Returns a iterator pointing to the past-the-end element of the result.
     */
    iterator end () {
        /* A completion-marked iterator */
        return iterator(*this, Result::ITER_END);
    };
};

}} /* namespace plcrash::async */

#endif /* PLCRASH_ASYNC_EITHER_H */
