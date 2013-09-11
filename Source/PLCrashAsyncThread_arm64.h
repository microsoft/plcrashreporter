/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008-2013 Plausible Labs Cooperative, Inc.
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

#ifndef PLCRASH_ASYNC_THREAD_ARM64_H
#define PLCRASH_ASYNC_THREAD_ARM64_H

#ifdef __cplusplus
extern "C" {
#endif
    
#ifdef __arm64__
    
// 64-bit
typedef uintptr_t plcrash_pdef_greg_t;
typedef uintptr_t plcrash_pdef_fpreg_t;
    
#endif /* __arm64__ */

/**
 * @internal
 * Arm registers
 */
typedef enum {
    /*
     * General
     */
    
    /** Program counter (r15) */
    PLCRASH_ARM64_PC = PLCRASH_REG_IP,
    
    /** Frame pointer */
    PLCRASH_ARM64_R7 = PLCRASH_REG_FP,
    
    /* stack pointer (r13) */
    PLCRASH_ARM64_SP = PLCRASH_REG_SP,
    
    PLCRASH_ARM64_R0,
    PLCRASH_ARM64_R1,
    PLCRASH_ARM64_R2,
    PLCRASH_ARM64_R3,
    PLCRASH_ARM64_R4,
    PLCRASH_ARM64_R5,
    PLCRASH_ARM64_R6,
    // R7 is defined above
    PLCRASH_ARM64_R8,
    PLCRASH_ARM64_R9,
    PLCRASH_ARM64_R10,
    PLCRASH_ARM64_R11,
    PLCRASH_ARM64_R12,
    
    // TODO - Incomplete; these were copied directly from the A32 code.
    
    /* link register (r14) */
    PLCRASH_ARM64_LR,
    
    /** Current program status register */
    PLCRASH_ARM64_CPSR,
    
    /** Last register */
    PLCRASH_ARM64_LAST_REG = PLCRASH_ARM64_CPSR
} plcrash_arm64_regnum_t;
    
#ifdef __cplusplus
}
#endif

#endif /* PLCRASH_ASYNC_THREAD_ARM64_H */
