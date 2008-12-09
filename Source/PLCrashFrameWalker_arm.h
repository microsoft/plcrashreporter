/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008 Plausible Labs Cooperative, Inc.
 * All rights reserved.
 */

#ifdef __arm__

// 32-bit
typedef int32_t plframe_pdef_word_t;
typedef int32_t plframe_pdef_fpreg_t;

/**
 * @internal
 * Arm registers
 */
typedef enum {

    /* Common registers */
    
    PLFRAME_PDEF_REG_IP = 0,
    
    /** Last register */
    PLFRAME_PDEF_LAST_REG = 0
} plframe_x86_regnum_t;

#endif