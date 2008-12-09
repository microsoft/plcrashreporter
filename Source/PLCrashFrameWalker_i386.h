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
    /*
     * General
     */
    
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
    PLFRAME_X86_ESP,

    /** Instruction pointer */
    PLFRAME_X86_EIP,
    
    /** Flags */
    PLFRAME_X86_EFLAGS,
    
    /* Scratcn */
    PLFRAME_X86_TRAPNO,
    
    
    /*
     * Segment Registers
     */
    /** Segment register */
    PLFRAME_X86_CS,
    
    /** Segment register */
    PLFRAME_X86_DS,
    
    /** Segment register */
    PLFRAME_X86_ES,
    
    /** Segment register */
    PLFRAME_X86_FS,
    
    /** Segment register */
    PLFRAME_X86_GS,
    
    /** Task register */
    PLFRAME_X86_TSS,
    
    /** LDT */
    PLFRAME_X86_LDT,
    
    /*
     * Floating Point
     */
    
    /** FP return value */
    PLFRAME_X86_ST0,

    /** Scratch */
    PLFRAME_X86_ST1,
    
    /** Scratch */
    PLFRAME_X86_ST2,
    
    /** Scratch */
    PLFRAME_X86_ST3,
    
    /** Scratch */
    PLFRAME_X86_ST4,
    
    /** Scratch */
    PLFRAME_X86_ST5,
    
    /** Scratch */
    PLFRAME_X86_ST6,
    
    /** Scratch */
    PLFRAME_X86_ST7,

    /*
     * SSE
     */
    /** Scratch */
    PLFRAME_X86_XMM0_lo,
    /** Scratch */
    PLFRAME_X86_XMM0_hi,

    /** Scratch */
    PLFRAME_X86_XMM1_lo,
    /** Scratch */
    PLFRAME_X86_XMM1_hi,

    /** Scratch */
    PLFRAME_X86_XMM2_lo,
    /** Scratch */
    PLFRAME_X86_XMM2_hi,

    /** Scratch */
    PLFRAME_X86_XMM3_lo,
    /** Scratch */
    PLFRAME_X86_XMM3_hi,

    /** Scratch */
    PLFRAME_X86_XMM4_lo,
    /** Scratch */
    PLFRAME_X86_XMM4_hi,

    /** Scratch */
    PLFRAME_X86_XMM5_lo,
    /** Scratch */
    PLFRAME_X86_XMM5_hi,

    /** Scratch */
    PLFRAME_X86_XMM6_lo,
    /** Scratch */
    PLFRAME_X86_XMM6_hi,

    /** Scratch */
    PLFRAME_X86_XMM7_lo,
    /** Scratch */
    PLFRAME_X86_XMM7_hi,

    /** Scratch */
    PLFRAME_X86_MXCSR,
    
    /* Common registers */

    PLFRAME_PDEF_REG_IP = PLFRAME_X86_EIP,
    
    /** Last register */
    PLFRAME_PDEF_LAST_REG = PLFRAME_X86_MXCSR
} plframe_x86_regnum_t;

#endif