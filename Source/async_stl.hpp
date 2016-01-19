/*
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

#ifndef PLCRASH_ASYNC_STL_HPP
#define PLCRASH_ASYNC_STL_HPP

#include "PLCrashMacros.h"

/**
 * @internal
 * @ingroup plcrash_async
 * @defgroup plcrash_async_stl STL-compatible compile-time-only templates and type traits.
 *
 * A set of STL-compatible templates and type traits for use in contexts where dependency on the C++
 * STL is not possible.
 *
 * @{
 */

PLCR_CPP_BEGIN_ASYNC_NS namespace atl {

/** Equivalent to C++11 std::nullptr_t */
typedef decltype(nullptr) nullptr_t;
    
/** Equivalent to C++11 std::integral_constant */
template <typename T, T Value> struct integral_constant {
    static constexpr const T value = Value;
    typedef T value_type;
    typedef integral_constant type;
    constexpr operator value_type() const noexcept {
        return value;
    }
};

/** Equivalent to C++17's std::bool_constant. */
template <bool B> using bool_constant = integral_constant<bool, B>;

/** Equivalent to C++11 std::false_type */
typedef integral_constant<bool, true> true_type;

/** Equivalent to C++11 std::false_type */
typedef integral_constant<bool, false> false_type;

/** Equivalent to C++11 std::conditional */
template <bool Cond, typename T, typename F> struct conditional     { typedef T type; };
template <typename T, typename F> struct conditional<false, T, F>   { typedef F type; };

/** Equivalent to C++11 std::is_lvalue_reference */
template <typename T> struct is_lvalue_reference       : false_type {};
template <typename T> struct is_lvalue_reference<T&>   : true_type {};

/** Equivalent to C++11 std::remove_reference */
template <typename R> struct remove_reference      { typedef R type; };
template <typename R> struct remove_reference<R&>  { typedef R type; };
template <typename R> struct remove_reference<R&&> { typedef R type; };

/** Equivalent to C++14 std::remove_reference_t */
template <typename T> using remove_reference_t = typename remove_reference<T>::type;
    
/* Equivalent to to C++14 std::forward */
template <typename T> constexpr T &&forward (remove_reference_t<T> &value)  { return static_cast<T&&>(value); }
template <typename T> constexpr T &&forward (remove_reference_t<T> &&value) {
    static_assert(!is_lvalue_reference<T>::value, "An rvalue reference can not be forwarded as an lvalue");
    return static_cast<T&&>(value);
}

/* Equivalent to C++14 std::move */
template <typename T> constexpr remove_reference_t<T> &&move (T&& value) noexcept {
    using V = remove_reference_t<T>;
    return static_cast<V&&>(value);
};
    
} /* namespace atl */ PLCR_CPP_END_ASYNC_NS

/**
 * @}
 */

#endif /* PLCRASH_ASYNC_STL_HPP */
