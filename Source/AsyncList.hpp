/*
 * Author: Landon Fuller <landonf@plausible.coop>
 *
 * Copyright (c) 2015 Plausible Labs Cooperative, Inc.
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

#ifndef PLCRASH_ASYNC_LINKED_LIST_HPP
#define PLCRASH_ASYNC_LINKED_LIST_HPP 1

#include "PLCrashMacros.h"

#include "AsyncAllocator.hpp"
#include "AsyncAllocatable.hpp"

#include "shared_ptr.hpp"
#include "async_stl.hpp"

PLCR_CPP_BEGIN_ASYNC_NS

/**
 * @internal
 * @ingroup plcrash_async
 *
 * An async-safe immutable list.
 *
 * @tparam T The list element type.
 */
template<typename T> class List : public AsyncAllocatable {
private:
    /**
     * A 'cons' cell containing an initial ('head') value, and a tail reference.
     */
    struct Cons : public AsyncAllocatable {
        /**
         * Construct a cell with both a head and a tail.
         *
         * @param head The initial element.
         * @param tail A reference to the tail elements.
         */
        Cons (T head, shared_ptr<const Cons> tail) : _head(head), _tail(atl::move(tail)) {}
        
        /**
         * Construct a single element cell with an empty list tail.
         *
         * @param head The single element to be included in this cell.
         */
        explicit Cons (T head) : _head(head) {}
        
        /** The cell's value. */
        T _head;
        
        /** The tail reference; may be NULL. */
        shared_ptr<const Cons> _tail;
    };
    
    /**
     * Construct a new list with a reference to an existing list's backing cons cell.
     *
     * @param cons An existing cons cell representing the list.
     */
    explicit List (shared_ptr<const Cons> cons) : _head(atl::move(cons)) {}

public:
    
    /**
     * Construct an empty list.
     */
    constexpr List () : _size(0) {}
    
    /**
     * Construct a single element list.
     *
     * @param allocator The allocator to be used for any internal allocations when constructing the list.
     * @param head The first (and only) element of the returned list.
     */
    List (AsyncAllocator *allocator, T head) : _head(make_shared<const Cons>(allocator, head)), _size(1) {}
    
    /**
     * Construct a list containing @a head, followed by @a tail.
     *
     * @param allocator The allocator to be used for any internal allocations when constructing the list.
     * @param head The first element of the returned list.
     * @param tail The tail elements of the returned list.
     */
    List (AsyncAllocator *allocator, T head, List const &tail) : _head(make_shared<const Cons>(allocator, head, tail._head)), _size(tail._size + 1) {}
    
    /** Copy constructor */
    List (const List &other) = default;
    
    /** Move constructor */
    List (List &&other) = default;
    
    /** Copy assignment operator */
    List &operator =(const List &other) = default;
    
    /** Move assignment operator */
    List &operator =(List &&other) = default;
    
    /**
     * Return true if this list is empty, false otherwise.
     */
    bool isEmpty () const { return !_head; }

    /**
     * Return the first value in the list. The behavior of this method on empty lists
     * is undefined.
     */
    T head () const {
        return _head->_head;
    }
    
    /**
     * Return the list tail.
     */
    List tail () const {
        /* If empty, no need to construct a new list. */
        if (isEmpty()) {
            return *this;
        } else {
            return List(_head->_tail);
        }
    }
    
    /**
     * Return a new list with the given value prepended to this list.
     *
     * @param allocator The allocator to be used for any internal allocations when constructing the new list.
     * @param value The value to prepend.
     */
    List prepend (AsyncAllocator *allocator, T value) {
        return List(allocator, value, *this);
    }

    /**
     * Return the number of elements in this list.
     */
    size_t size () { return _size; }
    
private:
    /** The first cell of this list, or an empty (NULL) value. */
    shared_ptr<const Cons> _head;
    
    /** The size of the list accessible via _head */
    size_t _size;
};


PLCR_CPP_END_ASYNC_NS


#endif /* PLCRASH_ASYNC_LINKED_LIST_HPP */
