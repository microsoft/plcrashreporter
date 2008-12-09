/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008 Plausible Labs Cooperative, Inc.
 * All rights reserved.
 */

#ifdef __i386__

// 32-bit
typedef int32_t plframe_pdef_word_t;
typedef int32_t plframe_pdef_fpreg_t;

/**
 * @internal
 * x86 registers, as defined by the System V ABI, IA32 Supplement. 
 */
typedef enum {
    /** Return value */
    PLFRAME_X86_EAX = 0,

    /** Dividend register */
    PLFRAME_X86_EDX,

    /** Count register */
    PLFRAME_X86_ECX,

    /** Local register variable */
    PLFRAME_X86_EBX,
    
    /** Stack frame pointer */
    PLFRAME_X86_EBP,

    /** Local register variable */
    PLFRAME_X86_ESI,

    /** Local register variable */
    PLFRAME_X86_EDI,

    /** Stack pointer */
    PLFRAME_X86_ESP
} plframe_x86_regnum_t;

#endif