/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008 Plausible Labs Cooperative, Inc.
 * All rights reserved.
 */

#ifdef __i386__

// 32-bit
typedef int32_t plframe_pdef_reg_t;
typedef int32_t plframe_pdef_fpreg_t;

// Data we'll read off the stack frame
#define PLFRAME_PDEF_STACKFRAME_LEN sizeof(intptr_t) * 2

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

    PLFRAME_PDEF_REG_IP = PLFRAME_X86_EIP,
    
    /** Last register */
    PLFRAME_PDEF_LAST_REG = PLFRAME_X86_GS
} plframe_x86_regnum_t;

#endif /* __i386__ */
