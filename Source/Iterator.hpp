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

#ifndef PLCRASH_ASYNC_ITERATOR_H
#define PLCRASH_ASYNC_ITERATOR_H

/**
 * @internal
 * @ingroup plcrash_async
 * @{
 */

namespace plcrash { namespace async {

/**
 * An async-safe data structure that supports forward iteration over a sequence of
 * elements, with support for C++11 range-based for loop syntax.
 *
 * Like C++ STL, termination of iteration is performed via comparison against a 'terminal'
 * iterator instance.
 *
 * @tparam T The iterator's value type.
 * @tparam Pointer Pointer type of T.
 * @tparam Reference Reference type of T.
 *
 * @note The design of this API -- specifically, use of increment, dereference, and equality operators --
 * may be considered suboptimal, but is necessary to maintain compatibility with C++11's
 * STL-style range-based for loop support.
 */
template <class T, class Pointer = const T*, class Reference = const T&> class Iterator {
public:
    /**
     * @warning This method is defined for automatic use in conjunction with C++11's ranged for loop syntax, and
     * should not be called directly.
     *
     * Advance the iterator by one position and return the new iterator value (prefix increment).
     */
    Iterator &operator++ ();
    
    /** Return a reference to the element at the iterator's current position. */
    Reference operator* () const;
    
    /** Return a pointer to the element at the iterator's current position. */
    Pointer operator-> () const;
    
    
    /**
     * @warning This method is defined for automatic use in conjunction with C++11's ranged for loop syntax, and
     * should not be called directly.
     *
     * Return true if this instance is not equal to @a other.
     *
     * @param other The iterator to compare against. This iterator must have been returned from the same iteration container.
     */
    bool operator!= (const Iterator &other) const;
};
    
}} /* namespace plcrash::async */


/** @} */

#endif /* PLCRASH_ASYNC_ITERATOR_H */
