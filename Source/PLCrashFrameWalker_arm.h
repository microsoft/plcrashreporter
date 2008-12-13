/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008 Plausible Labs Cooperative, Inc.
 * All rights reserved.
 */

#ifdef __arm__

// 32-bit
typedef uint32_t plframe_pdef_reg_t;
typedef uint32_t plframe_pdef_fpreg_t;

// Data we'll read off the stack frame
#define PLFRAME_PDEF_STACKFRAME_LEN sizeof(intptr_t) * 2

/**
 * @internal
 * Arm registers
 */
typedef enum {
    /*
     * General
     */

    PLFRAME_ARM_R0 = 0,
    PLFRAME_ARM_R1,
    PLFRAME_ARM_R2,
    PLFRAME_ARM_R3,
    PLFRAME_ARM_R4,
    PLFRAME_ARM_R5,
    PLFRAME_ARM_R6,
    PLFRAME_ARM_R7,
    PLFRAME_ARM_R8,
    PLFRAME_ARM_R9,
    PLFRAME_ARM_R10,
    PLFRAME_ARM_R11,
    PLFRAME_ARM_R12,

    /* stack pointer (r13) */
    PLFRAME_ARM_SP,

    /* link register (r14) */
    PLFRAME_ARM_LR,

    /** Program counter (r15) */
    PLFRAME_ARM_PC,

    /* Common registers */
    
    PLFRAME_PDEF_REG_IP = PLFRAME_ARM_PC,
    
    /** Last register */
    PLFRAME_PDEF_LAST_REG = PLFRAME_ARM_PC
} plframe_x86_regnum_t;

#endif