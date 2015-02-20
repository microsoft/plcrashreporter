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

#include "PLCrashAsyncThread.h"
#include "PLCrashAsync.h"

#include <signal.h>
#include <stdlib.h>
#include <assert.h>

#if defined(__arm__) || defined(__arm64__)

#define RETGEN(name, type, ts) {\
    return (ts->arm_state. type . __ ## name); \
}

#define SETGEN(name, type, ts, regnum, value) {\
    ts->valid_regs |= 1ULL<<regnum; \
    (ts->arm_state. type . __ ## name) = value; \
    break; \
}

/* Mapping of DWARF register numbers to PLCrashReporter register numbers. */
struct dwarf_register_table {
    /** Standard register number. */
    plcrash_regnum_t regnum;
    
    /** DWARF register number. */
    uint64_t dwarf_value;
};


/*
 * ARM GP registers defined as callee-preserved, as per Apple's iOS ARM Function Call Guide
 */
static const plcrash_regnum_t arm_nonvolatile_registers[] = {
    PLCRASH_ARM_R4,
    PLCRASH_ARM_R5,
    PLCRASH_ARM_R6,
    PLCRASH_ARM_R7,
    PLCRASH_ARM_R8,
    PLCRASH_ARM_R10,
    PLCRASH_ARM_R11,
};

/*
 * ARM GP registers defined as callee-preserved, as per ARM's Procedure Call Standard for the
 * ARM 64-bit Architecture (AArch64), 22nd May 2013.
 */
static const plcrash_regnum_t arm64_nonvolatile_registers[] = {
    PLCRASH_ARM64_X19,
    PLCRASH_ARM64_X20,
    PLCRASH_ARM64_X21,
    PLCRASH_ARM64_X22,
    PLCRASH_ARM64_X23,
    PLCRASH_ARM64_X24,
    PLCRASH_ARM64_X25,
    PLCRASH_ARM64_X26,
    PLCRASH_ARM64_X27,
    PLCRASH_ARM64_X28,

#ifdef __APPLE__
    // AAPCS 64 Section 5.2.3 allows an implementation to define the minimum
    // level of conformance with respect to maintaining frame records.
    //
    // Apple's ARM64 Function Calling Conventions states:
    // The frame pointer register (x29) must always address a valid frame record, although some functions—such
    // as leaf functions or tail calls—may elect not to create an entry in this list. As a result, stack traces will
    // always be meaningful, even without debug information.
    PLCRASH_ARM64_FP,
#else
#error Define OS frame pointer behavior as per AAPCS64 Section 5.2.3
#endif
};

/**
 * DWARF register mappings as defined in ARM's "DWARF for the ARM Architecture", ARM IHI 0040B,
 * issued November 30th, 2012.
 *
 * Note that not all registers have DWARF register numbers allocated, eg, the ARM standard states
 * in Section 3.1:
 *
 *   The CPSR, VFP and FPA control registers are not allocated a numbering above. It is
 *   considered unlikely that these will be needed for producing a stack back-trace in a
 *   debugger.
 */
static const struct dwarf_register_table arm_dwarf_table [] = {
    { PLCRASH_ARM_R0, 0 },
    { PLCRASH_ARM_R1, 1 },
    { PLCRASH_ARM_R2, 2 },
    { PLCRASH_ARM_R3, 3 },
    { PLCRASH_ARM_R4, 4 },
    { PLCRASH_ARM_R5, 5 },
    { PLCRASH_ARM_R6, 6 },
    { PLCRASH_ARM_R7, 7 },
    { PLCRASH_ARM_R8, 8 },
    { PLCRASH_ARM_R9, 9 },
    { PLCRASH_ARM_R10, 10 },
    { PLCRASH_ARM_R11, 11 },
    { PLCRASH_ARM_R12, 12 },
    { PLCRASH_ARM_SP, 13 },
    { PLCRASH_ARM_LR, 14 },
    { PLCRASH_ARM_PC, 15 }
};

/**
 * DWARF register mappings as defined in ARM's "DWARF for the ARM 64-bit Architecture (AArch64)", ARM IHI 0057B,
 * issued May 22nd, 2013.
 *
 * Note that not all registers have DWARF register numbers allocated, eg, the ARM standard states
 * in Section 3.1:
 *
 *   The CPSR, VFP and FPA control registers are not allocated a numbering above. It is
 *   considered unlikely that these will be needed for producing a stack back-trace in a
 *   debugger.
 */
static const struct dwarf_register_table arm64_dwarf_table [] = {
    // TODO_ARM64: These should be validated against actual arm64 DWARF data.
    { PLCRASH_ARM64_X0, 0 },
    { PLCRASH_ARM64_X1, 1 },
    { PLCRASH_ARM64_X2, 2 },
    { PLCRASH_ARM64_X3, 3 },
    { PLCRASH_ARM64_X4, 4 },
    { PLCRASH_ARM64_X5, 5 },
    { PLCRASH_ARM64_X6, 6 },
    { PLCRASH_ARM64_X7, 7 },
    { PLCRASH_ARM64_X8, 8 },
    { PLCRASH_ARM64_X9, 9 },
    { PLCRASH_ARM64_X10, 10 },
    { PLCRASH_ARM64_X11, 11 },
    { PLCRASH_ARM64_X12, 12 },
    { PLCRASH_ARM64_X13, 13 },
    { PLCRASH_ARM64_X14, 14 },
    { PLCRASH_ARM64_X15, 15 },
    { PLCRASH_ARM64_X16, 16 },
    { PLCRASH_ARM64_X17, 17 },
    { PLCRASH_ARM64_X18, 18 },
    { PLCRASH_ARM64_X19, 19 },
    { PLCRASH_ARM64_X20, 20 },
    { PLCRASH_ARM64_X21, 21 },
    { PLCRASH_ARM64_X22, 22 },
    { PLCRASH_ARM64_X23, 23 },
    { PLCRASH_ARM64_X24, 24 },
    { PLCRASH_ARM64_X25, 25 },
    { PLCRASH_ARM64_X26, 26 },
    { PLCRASH_ARM64_X27, 27 },
    { PLCRASH_ARM64_X28, 28 },
    { PLCRASH_ARM64_FP, 29 },
    { PLCRASH_ARM64_LR, 30 },
    
    { PLCRASH_ARM64_SP,  31 },
};



// PLCrashAsyncThread API
plcrash_greg_t plcrash_async_thread_state_get_reg (const plcrash_async_thread_state_t *ts, plcrash_regnum_t regnum) {
    if (ts->arm_state.thread.ash.flavor == ARM_THREAD_STATE32) {
        switch (regnum) {
            case PLCRASH_ARM_R0:
                RETGEN(r[0], thread.ts_32, ts);
                
            case PLCRASH_ARM_R1:
                RETGEN(r[1], thread.ts_32, ts);
                
            case PLCRASH_ARM_R2:
                RETGEN(r[2], thread.ts_32, ts);
                
            case PLCRASH_ARM_R3:
                RETGEN(r[3], thread.ts_32, ts);
                
            case PLCRASH_ARM_R4:
                RETGEN(r[4], thread.ts_32, ts);
                
            case PLCRASH_ARM_R5:
                RETGEN(r[5], thread.ts_32, ts);
                
            case PLCRASH_ARM_R6:
                RETGEN(r[6], thread.ts_32, ts);
                
            case PLCRASH_ARM_R7:
                RETGEN(r[7], thread.ts_32, ts);
                
            case PLCRASH_ARM_R8:
                RETGEN(r[8], thread.ts_32, ts);
                
            case PLCRASH_ARM_R9:
                RETGEN(r[9], thread.ts_32, ts);
                
            case PLCRASH_ARM_R10:
                RETGEN(r[10], thread.ts_32, ts);
                
            case PLCRASH_ARM_R11:
                RETGEN(r[11], thread.ts_32, ts);
                
            case PLCRASH_ARM_R12:
                RETGEN(r[12], thread.ts_32, ts);
                
            case PLCRASH_ARM_SP:
                RETGEN(sp, thread.ts_32, ts);
                
            case PLCRASH_ARM_LR:
                RETGEN(lr, thread.ts_32, ts);
                
            case PLCRASH_ARM_PC:
                RETGEN(pc, thread.ts_32, ts);
                
            case PLCRASH_ARM_CPSR:
                RETGEN(cpsr, thread.ts_32, ts);
                
            default:
                __builtin_trap();
        }
    } else {
        switch (regnum) {
            case PLCRASH_ARM64_X0:
                RETGEN(x[0], thread.ts_64, ts);
                
            case PLCRASH_ARM64_X1:
                RETGEN(x[1], thread.ts_64, ts);
                
            case PLCRASH_ARM64_X2:
                RETGEN(x[2], thread.ts_64, ts);
                
            case PLCRASH_ARM64_X3:
                RETGEN(x[3], thread.ts_64, ts);
                
            case PLCRASH_ARM64_X4:
                RETGEN(x[4], thread.ts_64, ts);
                
            case PLCRASH_ARM64_X5:
                RETGEN(x[5], thread.ts_64, ts);
                
            case PLCRASH_ARM64_X6:
                RETGEN(x[6], thread.ts_64, ts);
                
            case PLCRASH_ARM64_X7:
                RETGEN(x[7], thread.ts_64, ts);
                
            case PLCRASH_ARM64_X8:
                RETGEN(x[8], thread.ts_64, ts);
                
            case PLCRASH_ARM64_X9:
                RETGEN(x[9], thread.ts_64, ts);
                
            case PLCRASH_ARM64_X10:
                RETGEN(x[10], thread.ts_64, ts);
                
            case PLCRASH_ARM64_X11:
                RETGEN(x[11], thread.ts_64, ts);
                
            case PLCRASH_ARM64_X12:
                RETGEN(x[12], thread.ts_64, ts);
                
            case PLCRASH_ARM64_X13:
                RETGEN(x[13], thread.ts_64, ts);
                
            case PLCRASH_ARM64_X14:
                RETGEN(x[14], thread.ts_64, ts);
                
            case PLCRASH_ARM64_X15:
                RETGEN(x[15], thread.ts_64, ts);
                
            case PLCRASH_ARM64_X16:
                RETGEN(x[16], thread.ts_64, ts);
                
            case PLCRASH_ARM64_X17:
                RETGEN(x[17], thread.ts_64, ts);
                
            case PLCRASH_ARM64_X18:
                RETGEN(x[18], thread.ts_64, ts);
                
            case PLCRASH_ARM64_X19:
                RETGEN(x[19], thread.ts_64, ts);
                
            case PLCRASH_ARM64_X20:
                RETGEN(x[20], thread.ts_64, ts);
                
            case PLCRASH_ARM64_X21:
                RETGEN(x[21], thread.ts_64, ts);
                
            case PLCRASH_ARM64_X22:
                RETGEN(x[22], thread.ts_64, ts);
                
            case PLCRASH_ARM64_X23:
                RETGEN(x[23], thread.ts_64, ts);
                
            case PLCRASH_ARM64_X24:
                RETGEN(x[24], thread.ts_64, ts);
                
            case PLCRASH_ARM64_X25:
                RETGEN(x[25], thread.ts_64, ts);
                
            case PLCRASH_ARM64_X26:
                RETGEN(x[26], thread.ts_64, ts);
                
            case PLCRASH_ARM64_X27:
                RETGEN(x[27], thread.ts_64, ts);
                
            case PLCRASH_ARM64_X28:
                RETGEN(x[28], thread.ts_64, ts);
                
            case PLCRASH_ARM64_FP:
                RETGEN(fp, thread.ts_64, ts);
                
            case PLCRASH_ARM64_SP:
                RETGEN(sp, thread.ts_64, ts);
                
            case PLCRASH_ARM64_LR:
                RETGEN(lr, thread.ts_64, ts);
                
            case PLCRASH_ARM64_PC:
                RETGEN(pc, thread.ts_64, ts);
                
            case PLCRASH_ARM64_CPSR:
                RETGEN(cpsr, thread.ts_64, ts);
                
            default:
                __builtin_trap();
        }
    }
    
    /* Should not be reachable */
    return 0;
}

// PLCrashAsyncThread API
void plcrash_async_thread_state_set_reg (plcrash_async_thread_state_t *thread_state, plcrash_regnum_t regnum, plcrash_greg_t reg) {
    plcrash_async_thread_state_t *ts = thread_state;

    if (ts->arm_state.thread.ash.flavor == ARM_THREAD_STATE32) {
        switch (regnum) {
            case PLCRASH_ARM_R0:
                SETGEN(r[0], thread.ts_32, ts, regnum, reg);
                
            case PLCRASH_ARM_R1:
                SETGEN(r[1], thread.ts_32, ts, regnum, reg);
                
            case PLCRASH_ARM_R2:
                SETGEN(r[2], thread.ts_32, ts, regnum, reg);
                
            case PLCRASH_ARM_R3:
                SETGEN(r[3], thread.ts_32, ts, regnum, reg);
                
            case PLCRASH_ARM_R4:
                SETGEN(r[4], thread.ts_32, ts, regnum, reg);
                
            case PLCRASH_ARM_R5:
                SETGEN(r[5], thread.ts_32, ts, regnum, reg);
                
            case PLCRASH_ARM_R6:
                SETGEN(r[6], thread.ts_32, ts, regnum, reg);
                
            case PLCRASH_ARM_R7:
                SETGEN(r[7], thread.ts_32, ts, regnum, reg);
                
            case PLCRASH_ARM_R8:
                SETGEN(r[8], thread.ts_32, ts, regnum, reg);
                
            case PLCRASH_ARM_R9:
                SETGEN(r[9], thread.ts_32, ts, regnum, reg);
                
            case PLCRASH_ARM_R10:
                SETGEN(r[10], thread.ts_32, ts, regnum, reg);
                
            case PLCRASH_ARM_R11:
                SETGEN(r[11], thread.ts_32, ts, regnum, reg);
                
            case PLCRASH_ARM_R12:
                SETGEN(r[12], thread.ts_32, ts, regnum, reg);
                
            case PLCRASH_ARM_SP:
                SETGEN(sp, thread.ts_32, ts, regnum, reg);
                
            case PLCRASH_ARM_LR:
                SETGEN(lr, thread.ts_32, ts, regnum, reg);
                
            case PLCRASH_ARM_PC:
                SETGEN(pc, thread.ts_32, ts, regnum, reg);
                
            case PLCRASH_ARM_CPSR:
                SETGEN(cpsr, thread.ts_32, ts, regnum, reg);
                
            default:
                // Unsupported register
                __builtin_trap();
        }
    } else {
        switch (regnum) {
            case PLCRASH_ARM64_X0:
                SETGEN(x[0], thread.ts_64, ts, regnum, reg);
                
            case PLCRASH_ARM64_X1:
                SETGEN(x[1], thread.ts_64, ts, regnum, reg);
                
            case PLCRASH_ARM64_X2:
                SETGEN(x[2], thread.ts_64, ts, regnum, reg);
                
            case PLCRASH_ARM64_X3:
                SETGEN(x[3], thread.ts_64, ts, regnum, reg);
                
            case PLCRASH_ARM64_X4:
                SETGEN(x[4], thread.ts_64, ts, regnum, reg);
                
            case PLCRASH_ARM64_X5:
                SETGEN(x[5], thread.ts_64, ts, regnum, reg);
                
            case PLCRASH_ARM64_X6:
                SETGEN(x[6], thread.ts_64, ts, regnum, reg);
                
            case PLCRASH_ARM64_X7:
                SETGEN(x[7], thread.ts_64, ts, regnum, reg);
                
            case PLCRASH_ARM64_X8:
                SETGEN(x[8], thread.ts_64, ts, regnum, reg);
                
            case PLCRASH_ARM64_X9:
                SETGEN(x[9], thread.ts_64, ts, regnum, reg);
                
            case PLCRASH_ARM64_X10:
                SETGEN(x[10], thread.ts_64, ts, regnum, reg);
                
            case PLCRASH_ARM64_X11:
                SETGEN(x[11], thread.ts_64, ts, regnum, reg);
                
            case PLCRASH_ARM64_X12:
                SETGEN(x[12], thread.ts_64, ts, regnum, reg);
                
            case PLCRASH_ARM64_X13:
                SETGEN(x[13], thread.ts_64, ts, regnum, reg);
                
            case PLCRASH_ARM64_X14:
                SETGEN(x[14], thread.ts_64, ts, regnum, reg);
                
            case PLCRASH_ARM64_X15:
                SETGEN(x[15], thread.ts_64, ts, regnum, reg);
                
            case PLCRASH_ARM64_X16:
                SETGEN(x[16], thread.ts_64, ts, regnum, reg);
                
            case PLCRASH_ARM64_X17:
                SETGEN(x[17], thread.ts_64, ts, regnum, reg);
                
            case PLCRASH_ARM64_X18:
                SETGEN(x[18], thread.ts_64, ts, regnum, reg);
                
            case PLCRASH_ARM64_X19:
                SETGEN(x[19], thread.ts_64, ts, regnum, reg);
                
            case PLCRASH_ARM64_X20:
                SETGEN(x[20], thread.ts_64, ts, regnum, reg);
                
            case PLCRASH_ARM64_X21:
                SETGEN(x[21], thread.ts_64, ts, regnum, reg);
                
            case PLCRASH_ARM64_X22:
                SETGEN(x[22], thread.ts_64, ts, regnum, reg);
                
            case PLCRASH_ARM64_X23:
                SETGEN(x[23], thread.ts_64, ts, regnum, reg);
                
            case PLCRASH_ARM64_X24:
                SETGEN(x[24], thread.ts_64, ts, regnum, reg);
                
            case PLCRASH_ARM64_X25:
                SETGEN(x[25], thread.ts_64, ts, regnum, reg);
                
            case PLCRASH_ARM64_X26:
                SETGEN(x[26], thread.ts_64, ts, regnum, reg);
                
            case PLCRASH_ARM64_X27:
                SETGEN(x[27], thread.ts_64, ts, regnum, reg);
                
            case PLCRASH_ARM64_X28:
                SETGEN(x[28], thread.ts_64, ts, regnum, reg);
                
            case PLCRASH_ARM64_FP:
                SETGEN(fp, thread.ts_64, ts, regnum, reg);
                
            case PLCRASH_ARM64_SP:
                SETGEN(sp, thread.ts_64, ts, regnum, reg);
                
            case PLCRASH_ARM64_LR:
                SETGEN(lr, thread.ts_64, ts, regnum, reg);
                
            case PLCRASH_ARM64_PC:
                SETGEN(pc, thread.ts_64, ts, regnum, reg);
                
            case PLCRASH_ARM64_CPSR:
                SETGEN(cpsr, thread.ts_64, ts, regnum, reg);
                
            default:
                __builtin_trap();
        }
    }
}

// PLCrashAsyncThread API
size_t plcrash_async_thread_state_get_reg_count (const plcrash_async_thread_state_t *thread_state) {
    /* Last is an index value, so increment to get the count */
    if (thread_state->arm_state.thread.ash.flavor == ARM_THREAD_STATE32) {
        return PLCRASH_ARM_LAST_REG+1;
    } else {
        return PLCRASH_ARM64_LAST_REG+1;
    }
}

// PLCrashAsyncThread API
char const *plcrash_async_thread_state_get_reg_name (const plcrash_async_thread_state_t *thread_state, plcrash_regnum_t regnum) {
    if (thread_state->arm_state.thread.ash.flavor == ARM_THREAD_STATE32) {
        switch ((plcrash_arm_regnum_t) regnum) {
            case PLCRASH_ARM_R0:
                return "r0";
            case PLCRASH_ARM_R1:
                return "r1";
            case PLCRASH_ARM_R2:
                return "r2";
            case PLCRASH_ARM_R3:
                return "r3";
            case PLCRASH_ARM_R4:
                return "r4";
            case PLCRASH_ARM_R5:
                return "r5";
            case PLCRASH_ARM_R6:
                return "r6";
            case PLCRASH_ARM_R7:
                return "r7";
            case PLCRASH_ARM_R8:
                return "r8";
            case PLCRASH_ARM_R9:
                return "r9";
            case PLCRASH_ARM_R10:
                return "r10";
            case PLCRASH_ARM_R11:
                return "r11";
            case PLCRASH_ARM_R12:
                return "r12";
                
            case PLCRASH_ARM_SP:
                return "sp";
                
            case PLCRASH_ARM_LR:
                return "lr";
                
            case PLCRASH_ARM_PC:
                return "pc";
                
            case PLCRASH_ARM_CPSR:
                return "cpsr";
        }
    } else {
        switch ((plcrash_arm64_regnum_t) regnum) {
            case PLCRASH_ARM64_X0:
                return "x0";
                
            case PLCRASH_ARM64_X1:
                return "x1";
                
            case PLCRASH_ARM64_X2:
                return "x2";
                
            case PLCRASH_ARM64_X3:
                return "x3";
                
            case PLCRASH_ARM64_X4:
                return "x4";
                
            case PLCRASH_ARM64_X5:
                return "x5";
                
            case PLCRASH_ARM64_X6:
                return "x6";
                
            case PLCRASH_ARM64_X7:
                return "x7";
                
            case PLCRASH_ARM64_X8:
                return "x8";
                
            case PLCRASH_ARM64_X9:
                return "x9";
                
            case PLCRASH_ARM64_X10:
                return "x10";
                
            case PLCRASH_ARM64_X11:
                return "x11";
                
            case PLCRASH_ARM64_X12:
                return "x12";
                
            case PLCRASH_ARM64_X13:
                return "x13";
                
            case PLCRASH_ARM64_X14:
                return "x14";
                
            case PLCRASH_ARM64_X15:
                return "x15";
                
            case PLCRASH_ARM64_X16:
                return "x16";
                
            case PLCRASH_ARM64_X17:
                return "x17";
                
            case PLCRASH_ARM64_X18:
                return "x18";
                
            case PLCRASH_ARM64_X19:
                return "x19";
                
            case PLCRASH_ARM64_X20:
                return "x20";
                
            case PLCRASH_ARM64_X21:
                return "x21";
                
            case PLCRASH_ARM64_X22:
                return "x22";
                
            case PLCRASH_ARM64_X23:
                return "x23";
                
            case PLCRASH_ARM64_X24:
                return "x24";
                
            case PLCRASH_ARM64_X25:
                return "x25";
                
            case PLCRASH_ARM64_X26:
                return "x26";
                
            case PLCRASH_ARM64_X27:
                return "x27";
                
            case PLCRASH_ARM64_X28:
                return "x28";
                
            case PLCRASH_ARM64_FP:
                return "fp";
                
            case PLCRASH_ARM64_SP:
                return "sp";
                
            case PLCRASH_ARM64_LR:
                return "lr";
                
            case PLCRASH_ARM64_PC:
                return "pc";
                
            case PLCRASH_ARM64_CPSR:
                return "cpsr";
        }
    }
    
    /* Unsupported register is an implementation error (checked in unit tests) */
    PLCF_DEBUG("Missing register name for register id: %d", regnum);
    abort();
}

// PLCrashAsyncThread API
void plcrash_async_thread_state_clear_volatile_regs (plcrash_async_thread_state_t *thread_state) {
    const plcrash_regnum_t *table;
    size_t table_count = 0;
    
    if (thread_state->arm_state.thread.ash.flavor == ARM_THREAD_STATE32) {
        table = arm_nonvolatile_registers;
        table_count = sizeof(arm_nonvolatile_registers) / sizeof(arm_nonvolatile_registers[0]);
    } else {
        table = arm64_nonvolatile_registers;
        table_count = sizeof(arm64_nonvolatile_registers) / sizeof(arm64_nonvolatile_registers[0]);
    }
    
    size_t reg_count = plcrash_async_thread_state_get_reg_count(thread_state);
    for (size_t reg = 0; reg < reg_count; reg++) {
        /* Skip unset registers */
        if (!plcrash_async_thread_state_has_reg(thread_state, reg))
            continue;
        
        /* Check for the register in the preservation table */
        bool preserved = false;
        for (size_t i = 0; i < table_count; i++) {
            if (table[i] == reg) {
                preserved = true;
                break;
            }
        }
        
        /* If not preserved, clear */
        if (!preserved)
            plcrash_async_thread_state_clear_reg(thread_state, reg);
    }
}

// PLCrashAsyncThread API
bool plcrash_async_thread_state_map_reg_to_dwarf (plcrash_async_thread_state_t *thread_state, plcrash_regnum_t regnum, uint64_t *dwarf_reg) {
    const struct dwarf_register_table *table;
    size_t table_count = 0;
    
    if (thread_state->arm_state.thread.ash.flavor == ARM_THREAD_STATE32) {
        table = arm_dwarf_table;
        table_count = sizeof(arm_dwarf_table) / sizeof(arm_dwarf_table[0]);
    } else {
        table = arm64_dwarf_table;
        table_count = sizeof(arm64_dwarf_table) / sizeof(arm64_dwarf_table[0]);
    }
    
    for (size_t i = 0; i < table_count; i++) {
        if (table[i].regnum == regnum) {
            *dwarf_reg = table[i].dwarf_value;
            return true;
        }
    }
    
    /* Unknown register.  */
    return false;
}

// PLCrashAsyncThread API
bool plcrash_async_thread_state_map_dwarf_to_reg (const plcrash_async_thread_state_t *thread_state, uint64_t dwarf_reg, plcrash_regnum_t *regnum) {
    const struct dwarf_register_table *table;
    size_t table_count = 0;
    
    if (thread_state->arm_state.thread.ash.flavor == ARM_THREAD_STATE32) {
        table = arm_dwarf_table;
        table_count = sizeof(arm_dwarf_table) / sizeof(arm_dwarf_table[0]);
    } else {
        table = arm64_dwarf_table;
        table_count = sizeof(arm64_dwarf_table) / sizeof(arm64_dwarf_table[0]);
    }
    
    for (size_t i = 0; i < table_count; i++) {
        if (table[i].dwarf_value == dwarf_reg) {
            *regnum = table[i].regnum;
            return true;
        }
    }
    
    /* Unknown DWARF register.  */
    return false;
}

#endif /* __arm__ || __arm64__ */
