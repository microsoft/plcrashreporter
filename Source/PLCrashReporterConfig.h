/*
 * Author: Landon Fuller <landonf@plausible.coop>
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

#import <Foundation/Foundation.h>

/**
 * @ingroup enums
 * Supported mechanisms for trapping and handling crashes.
 */
typedef NS_ENUM(NSUInteger, PLCrashReporterConfigSignalHandler) {
    /**
     * Trap fatal signals via a sigaction(2)-registered BSD signal handler.
     *
     * There are some limitations to signal-based crash handling on Mac OS X and iOS; specifically:
     *
     * - On Mac OS X, stack overflows will only be handled on the thread on which
     *   the crash reporter was initialized. This should generally be the main thread.
     * - On iOS 6.0 and later, any stack overflows will not be handled due to sigaltstack() being
     *   non-functional on the device. (see rdar://13002712 - SA_ONSTACK/sigaltstack() ignored on iOS).
     * - Some exit paths in Apple's Libc will deregister a signal handler before firing SIGABRT, resulting
     *   in the signal handler never being called (see rdar://14313497 - ___abort() disables SIGABRT signal
     *   handlers prior to raising SIGABRT).  These __abort()-based checks are:
     *     - Implemented for unsafe memcpy/strcpy/snprintf C functions.
     *     - Only enabled when operating on a fixed-width target buffer (in which case the
     *       compiler rewrites the function calls to the built-in variants, and provides the fixed-width length as an argument).
     *     - Only trigger in the case that the source data exceeds the size of the fixed width target
     *       buffer, and the maximum length argument either isn't supplied by the caller (eg, when using strcpy),
     *       or a too-long argument is supplied (eg, strncpy with a length argument longer than the target buffer),
     *       AND that argument can't be checked at compile-time.
     */
    PLCrashReporterConfigSignalHandlerBSD = 0,

    /**
     * Trap fatal signals via a Mach exception server.
     *
     * If true, enable Mach exception support. On Mac OS X, the Mach exception implementation is fully supported,
     * using publicly available API -- note, however, that some kernel-internal constants, as well as
     * architecture-specific trap information, may be required to fully interpret a Mach exception's root cause.
     *
     * On iOS, the APIs required for a complete implementation are not public.
     *
     * The exposed surface of undocumented API usage is relatively low, and there has been strong user demand to
     * implement Mach exception handling regardless of concerns over API visiblity. Given this, we've included
     * Mach exception handling as an optional feature, with both build-time and runtime configuration
     * to disable its inclusion or use, respectively.
     *
     * For more information, refer to @ref mach_exceptions.
     */
    PLCrashReporterConfigSignalHandlerMach = 1
};

@interface PLCrashReporterConfig : NSObject

@end
