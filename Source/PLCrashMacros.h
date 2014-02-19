/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2013 Plausible Labs Cooperative, Inc.
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

#ifndef PLCRASH_CONSTANTS_H
#define PLCRASH_CONSTANTS_H

#if defined(__cplusplus)
#   define PLCR_EXPORT extern "C"
#   define PLCR_C_BEGIN_DECLS extern "C" {
#   define PLCR_C_END_DECLS }
#else
#   define PLCR_EXPORT extern
#   define PLCR_C_BEGIN_DECLS
#   define PLCR_C_END_DECLS
#endif

#ifdef __clang__
#  define PLCR_PRAGMA_CLANG(_p) _Pragma(_p)
#else
#  define PLCR_PRAGMA_CLANG(_p)
#endif

#if (__cplusplus && __cplusplus >= 201103L) || (!__cplusplus && defined(__has_feature) && __has_feature(objc_fixed_enum))
/**
 * Declare a C++11 / Objective-C / C enum with a fixed storage type, which may be used to represent
 * a set of bitfield flags.
 *
 * @param enum The enum name.
 * @param type The enum type.
 */
#define PLCF_ENUM_FLAGS(_name, _type) enum _name : _type _name; enum _name : _type
#else
#define PLCF_ENUM_FLAGS(_name, _type) _type _name; enum
#endif

#endif /* PLCRASH_CONSTANTS_H */
