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

#include "PLCrashFrameWalkerRegisterSet.h"

/**
 * @internal
 * @ingroup plcrash_backtrace_private
 * @{
 */

/**
 * Set @a regnum in @a set.
 *
 * @param set The set to modify.
 * @param regnum The register to set.
 */
void plframe_regset_set (plframe_regset_t *set, plcrash_regnum_t regnum) {
    *set = *set | (1<<regnum);
}

/**
 * Enable all registers in @a set.
 *
 * @param set The set to modify.
 */
void plframe_regset_set_all (plframe_regset_t *set) {
    memset(set, 0xFF, sizeof(*set));
}

/**
 * Return true if @a regnum is set in @a set, false otherwise.
 *
 * @param set The set to test.
 * @param regnum The register number to test for.
 */
bool plframe_regset_isset (plframe_regset_t set, plcrash_regnum_t regnum) {
    if ((set & (1<<regnum)) != 0)
        return true;
    
    return false;
}

/**
 * Unset @a regnum in @a set.
 *
 * @param set The set to modify.
 * @param regnum The register to unset.
 */
void plframe_regset_unset (plframe_regset_t *set, plcrash_regnum_t regnum) {
    *set &= ~(1<<regnum);
}

/**
 * Zero all registers in @a set.
 *
 * @param set The set to modify.
 */
void plframe_regset_zero (plframe_regset_t *set) {
    *set = 0;
}

/**
 * @}
 */