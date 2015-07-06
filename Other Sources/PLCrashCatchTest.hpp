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

#ifndef PLCRASH_CATCH_TEST_HPP
#define PLCRASH_CATCH_TEST_HPP

#include "async_stl.hpp"

/*
 * Provide a C++11-compatible nullptr_t for use in Catch; this is necessary to avoid overload ambiguity
 * in catch caused by our own usage of atl::nullptr_t.
 *
 * If we adopt a minimum requirement of a C++11-capable STL (which will require that we drop
 * support for Mac OS X 10.6), this should be removed.
 */
#define CATCH_CONFIG_CPP11_NULLPTR
namespace std {
    inline namespace _async_compat {
        typedef plcrash::async::atl::nullptr_t nullptr_t;
    }
}


#include "catch.hpp"

#endif /* PLCRASH_CATCH_TEST_HPP */
