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

#ifndef PLCRASH_DLCOMPAT_H
#define PLCRASH_DLCOMPAT_H 1

#include "PLCrashMacros.h"
#include <dlfcn.h>

/**
 * @internal
 *
 * @ingroup plcrash_internal
 * @defgroup plcrash_test_dlfcn dlfcn Compatibility APIs
 *
 * In iOS 9 beta 2 (current at the time of this writing), regressions in dyld have resulted in the
 * dlfcn APIs not working reliably, if at all.
 *
 * We rely on dlfcn in a number of locations in our unit tests; these functions provide a drop-in replacement
 * that leverage PLCrashReporter's own, working image lookup and symbolication code.
 *
 * @{
 */

PLCR_C_BEGIN_DECLS

int pl_dladdr (const void *addr, Dl_info *info);

PLCR_C_END_DECLS

/**
 * @}
 */


#endif /* PLCRASH_DLCOMPAT_H */
