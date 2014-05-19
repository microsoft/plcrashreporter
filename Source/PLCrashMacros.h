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

#ifdef __clang__
#  define PLCR_DEPRECATED __attribute__((deprecated))
#else
#  define PLCR_DEPRECATED
#endif

#ifdef PLCR_PRIVATE
/**
 * Marks a definition as deprecated only for for external clients, allowing
 * uses of it internal fo the framework.
 */
#define PLCR_EXTERNAL_DEPRECATED

/**
 * @internal
 * A macro to put above a definition marked PLCR_EXTERNAL_DEPRECATED that will
 * silence warnings about there being a deprecation documentation marker but the
 * definition not being marked deprecated.
 */
#  define PLCR_EXTERNAL_DEPRECATED_NOWARN_PUSH() \
      PLCR_PRAGMA_CLANG("clang diagnostic push"); \
      PLCR_PRAGMA_CLANG("clang diagnostic ignored \"-Wdocumentation-deprecated-sync\"")

/**
 * @internal
 * A macro to put below a definition marked PLCR_EXTERNAL_DEPRECATED that will
 * silence warnings about there being a deprecation documentation marker but the
 * definition not being marked deprecated.
 */
#  define PLCR_EXTERNAL_DEPRECATED_NOWARN_POP() PLCR_PRAGMA_CLANG("clang diagnostic pop")
#else
#  define PLCR_EXTERNAL_DEPRECATED PLCR_DEPRECATED
#  define PLCR_EXTERNAL_DEPRECATED_NOWARN_PUSH()
#  define PLCR_EXTERNAL_DEPRECATED_NOWARN_PUSH()
#endif /* PLCR_PRIVATE */

#ifdef PLCR_PRIVATE
#  if defined(__clang__) && __has_feature(cxx_attributes) && __has_warning("-Wimplicit-fallthrough")
#    define PLCR_FALLTHROUGH [[clang::fallthrough]]
#  else
#    define PLCR_FALLTHROUGH do {} while (0)
#  endif
#endif

#endif /* PLCRASH_CONSTANTS_H */
