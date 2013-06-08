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

#import "PLCrashAsyncThread.h"
#import "PLCrashAsync.h"

#import <signal.h>
#import <assert.h>
#import <stdlib.h>

#define RETGEN(name, type, ts) {\
    return (ts->x86_state. type . __ ## name); \
}

#define SETGEN(name, type, ts, regnum, value) {\
    ts->valid_regs |= 1<<regnum; \
    (ts->x86_state. type . __ ## name) = value; \
    break; \
}

#if defined(__i386__) || defined(__x86_64__)

static plcrash_greg_t plcrash_async_thread_state_get_reg_32 (const plcrash_async_thread_state_t *thread_state, plcrash_regnum_t regnum);
static plcrash_greg_t plcrash_async_thread_state_get_reg_64 (const plcrash_async_thread_state_t *thread_state, plcrash_regnum_t regnum);

static void plcrash_async_thread_state_set_reg_32 (plcrash_async_thread_state_t *cursor, plcrash_regnum_t regnum, plcrash_greg_t reg);
static void plcrash_async_thread_state_set_reg_64 (plcrash_async_thread_state_t *cursor, plcrash_regnum_t regnum, plcrash_greg_t reg);

static const char *plcrash_async_thread_state_get_regname_32 (plcrash_regnum_t regnum);
static const char *plcrash_async_thread_state_get_regname_64 (plcrash_regnum_t regnum);

plcrash_regnum_t plcrash_async_thread_state_map_dwarf_reg_32 (uint64_t dwarf_reg);
plcrash_regnum_t plcrash_async_thread_state_map_dwarf_reg_64 (uint64_t dwarf_reg);

// PLCrashAsyncThread API
plcrash_greg_t plcrash_async_thread_state_get_reg (const plcrash_async_thread_state_t *thread_state, plcrash_regnum_t regnum) {
    if (thread_state->x86_state.thread.tsh.flavor == x86_THREAD_STATE32) {
        return plcrash_async_thread_state_get_reg_32(thread_state, regnum);
    } else {
        return plcrash_async_thread_state_get_reg_64(thread_state, regnum);
    }
}

// PLCrashAsyncThread API
void plcrash_async_thread_state_set_reg (plcrash_async_thread_state_t *thread_state, plcrash_regnum_t regnum, plcrash_greg_t reg) {
    if (thread_state->x86_state.thread.tsh.flavor == x86_THREAD_STATE32) {
        return plcrash_async_thread_state_set_reg_32(thread_state, regnum, reg);
    } else {
        return plcrash_async_thread_state_set_reg_64(thread_state, regnum, reg);
    }
}

// PLCrashAsyncThread API
char const *plcrash_async_thread_state_get_reg_name (const plcrash_async_thread_state_t *thread_state, plcrash_regnum_t regnum) {
    if (thread_state->x86_state.thread.tsh.flavor == x86_THREAD_STATE32) {
        return plcrash_async_thread_state_get_regname_32(regnum);
    } else {
        return plcrash_async_thread_state_get_regname_64(regnum);
    }
}

// PLCrashAsyncThread API
size_t plcrash_async_thread_state_get_reg_count (const plcrash_async_thread_state_t *thread_state) {
    /* Last is an index value, so increment to get the count */
    if (thread_state->x86_state.thread.tsh.flavor == x86_THREAD_STATE32) {
        return PLCRASH_X86_LAST_REG+1;
    } else {
        return PLCRASH_X86_64_LAST_REG+1;
    }
}

// PLCrashAsyncThread API
plcrash_regnum_t plcrash_async_thread_state_map_dwarf_reg (const plcrash_async_thread_state_t *thread_state, uint64_t dwarf_reg) {
    if (thread_state->x86_state.thread.tsh.flavor == x86_THREAD_STATE32) {
        return plcrash_async_thread_state_map_dwarf_reg_32(dwarf_reg);
    } else {
        return plcrash_async_thread_state_map_dwarf_reg_64(dwarf_reg);
    }
}

/**
 * @internal
 * 32-bit implementation of plcrash_async_thread_state_get_reg()
 */
static plcrash_greg_t plcrash_async_thread_state_get_reg_32 (const plcrash_async_thread_state_t *thread_state, plcrash_regnum_t regnum) {
    const plcrash_async_thread_state_t *ts = thread_state;

    /* All word-sized registers */
    switch (regnum) {
        case PLCRASH_X86_EAX:
            RETGEN(eax, thread.uts.ts32, ts);
            
        case PLCRASH_X86_EDX:
            RETGEN(edx, thread.uts.ts32, ts);
            
        case PLCRASH_X86_ECX:
            RETGEN(ecx, thread.uts.ts32, ts);
            
        case PLCRASH_X86_EBX:
            RETGEN(ebx, thread.uts.ts32, ts);
            
        case PLCRASH_X86_EBP:
            RETGEN(ebp, thread.uts.ts32, ts);
            
        case PLCRASH_X86_ESI:
            RETGEN(esi, thread.uts.ts32, ts);
            
        case PLCRASH_X86_EDI:
            RETGEN(edi, thread.uts.ts32, ts);
            
        case PLCRASH_X86_ESP:
            RETGEN(esp, thread.uts.ts32, ts);
            
        case PLCRASH_X86_EIP:
            RETGEN(eip, thread.uts.ts32, ts);
            
        case PLCRASH_X86_EFLAGS:
            RETGEN(eflags, thread.uts.ts32, ts);
            
        case PLCRASH_X86_TRAPNO:
            RETGEN(trapno, exception.ues.es32, ts);
            
        case PLCRASH_X86_CS:
            RETGEN(cs, thread.uts.ts32, ts);
            
        case PLCRASH_X86_DS:
            RETGEN(ds, thread.uts.ts32, ts);
            
        case PLCRASH_X86_ES:
            RETGEN(es, thread.uts.ts32, ts);
            
        case PLCRASH_X86_FS:
            RETGEN(fs, thread.uts.ts32, ts);
            
        case PLCRASH_X86_GS:
            RETGEN(gs, thread.uts.ts32, ts);
            
        default:
            // Unsupported register
            __builtin_trap();
    }
    
    /* Shouldn't be reachable */
    return 0;
}

/**
 * @internal
 * 64-bit implementation of plcrash_async_thread_state_get_reg()
 */
static plcrash_greg_t plcrash_async_thread_state_get_reg_64 (const plcrash_async_thread_state_t *thread_state, plcrash_regnum_t regnum) {
    const plcrash_async_thread_state_t *ts = thread_state;

    switch (regnum) {
        case PLCRASH_X86_64_RAX:
            RETGEN(rax, thread.uts.ts64, ts);
            
        case PLCRASH_X86_64_RBX:
            RETGEN(rbx, thread.uts.ts64, ts);
            
        case PLCRASH_X86_64_RCX:
            RETGEN(rcx, thread.uts.ts64, ts);
            
        case PLCRASH_X86_64_RDX:
            RETGEN(rdx, thread.uts.ts64, ts);
            
        case PLCRASH_X86_64_RDI:
            RETGEN(rdi, thread.uts.ts64, ts);
            
        case PLCRASH_X86_64_RSI:
            RETGEN(rsi, thread.uts.ts64, ts);
            
        case PLCRASH_X86_64_RBP:
            RETGEN(rbp, thread.uts.ts64, ts);
            
        case PLCRASH_X86_64_RSP:
            RETGEN(rsp, thread.uts.ts64, ts);
            
        case PLCRASH_X86_64_R8:
            RETGEN(r8, thread.uts.ts64, ts);
            
        case PLCRASH_X86_64_R9:
            RETGEN(r9, thread.uts.ts64, ts);
            
        case PLCRASH_X86_64_R10:
            RETGEN(r10, thread.uts.ts64, ts);
            
        case PLCRASH_X86_64_R11:
            RETGEN(r11, thread.uts.ts64, ts);
            
        case PLCRASH_X86_64_R12:
            RETGEN(r12, thread.uts.ts64, ts);
            
        case PLCRASH_X86_64_R13:
            RETGEN(r13, thread.uts.ts64, ts);
            
        case PLCRASH_X86_64_R14:
            RETGEN(r14, thread.uts.ts64, ts);
            
        case PLCRASH_X86_64_R15:
            RETGEN(r15, thread.uts.ts64, ts);
            
        case PLCRASH_X86_64_RIP:
            RETGEN(rip, thread.uts.ts64, ts);
            
        case PLCRASH_X86_64_RFLAGS:
            RETGEN(rflags, thread.uts.ts64, ts);
            
        case PLCRASH_X86_64_CS:
            RETGEN(cs, thread.uts.ts64, ts);
            
        case PLCRASH_X86_64_FS:
            RETGEN(fs, thread.uts.ts64, ts);
            
        case PLCRASH_X86_64_GS:
            RETGEN(gs, thread.uts.ts64, ts);
            
        default:
            // Unsupported register
            __builtin_trap();
    }

    /* Should not be reachable */
    return 0;
}

/**
 * @internal
 * 32-bit implementation of plcrash_async_thread_state_set_reg()
 */
static void plcrash_async_thread_state_set_reg_32 (plcrash_async_thread_state_t *thread_state, plcrash_regnum_t regnum, plcrash_greg_t reg) {
    plcrash_async_thread_state_t *ts = thread_state;
    
    /* All word-sized registers */
    switch (regnum) {
        case PLCRASH_X86_EAX:
            SETGEN(eax, thread.uts.ts32, ts, regnum, reg);
            
        case PLCRASH_X86_EDX:
            SETGEN(edx, thread.uts.ts32, ts, regnum, reg);
            
        case PLCRASH_X86_ECX:
            SETGEN(ecx, thread.uts.ts32, ts, regnum, reg);
            
        case PLCRASH_X86_EBX:
            SETGEN(ebx, thread.uts.ts32, ts, regnum, reg);
            
        case PLCRASH_X86_EBP:
            SETGEN(ebp, thread.uts.ts32, ts, regnum, reg);
            
        case PLCRASH_X86_ESI:
            SETGEN(esi, thread.uts.ts32, ts, regnum, reg);
            
        case PLCRASH_X86_EDI:
            SETGEN(edi, thread.uts.ts32, ts, regnum, reg);
            
        case PLCRASH_X86_ESP:
            SETGEN(esp, thread.uts.ts32, ts, regnum, reg);
            
        case PLCRASH_X86_EIP:
            SETGEN(eip, thread.uts.ts32, ts, regnum, reg);
            
        case PLCRASH_X86_EFLAGS:
            SETGEN(eflags, thread.uts.ts32, ts, regnum, reg);
            
        case PLCRASH_X86_TRAPNO:
            SETGEN(trapno, exception.ues.es32, ts, regnum, reg);
            
        case PLCRASH_X86_CS:
            SETGEN(cs, thread.uts.ts32, ts, regnum, reg);
            
        case PLCRASH_X86_DS:
            SETGEN(ds, thread.uts.ts32, ts, regnum, reg);
            
        case PLCRASH_X86_ES:
            SETGEN(es, thread.uts.ts32, ts, regnum, reg);
            
        case PLCRASH_X86_FS:
            SETGEN(fs, thread.uts.ts32, ts, regnum, reg);
            
        case PLCRASH_X86_GS:
            SETGEN(gs, thread.uts.ts32, ts, regnum, reg);
            
        default:
            // Unsupported register
            __builtin_trap();
    }
}

/**
 * @internal
 * 64-bit implementation of plcrash_async_thread_state_set_reg()
 */
static void plcrash_async_thread_state_set_reg_64 (plcrash_async_thread_state_t *thread_state, plcrash_regnum_t regnum, plcrash_greg_t reg) {
    plcrash_async_thread_state_t *ts = thread_state;
    
    switch (regnum) {
        case PLCRASH_X86_64_RAX:
            SETGEN(rax, thread.uts.ts64, ts, regnum, reg);
            
        case PLCRASH_X86_64_RBX:
            SETGEN(rbx, thread.uts.ts64, ts, regnum, reg);
            
        case PLCRASH_X86_64_RCX:
            SETGEN(rcx, thread.uts.ts64, ts, regnum, reg);
            
        case PLCRASH_X86_64_RDX:
            SETGEN(rdx, thread.uts.ts64, ts, regnum, reg);
            
        case PLCRASH_X86_64_RDI:
            SETGEN(rdi, thread.uts.ts64, ts, regnum, reg);
            
        case PLCRASH_X86_64_RSI:
            SETGEN(rsi, thread.uts.ts64, ts, regnum, reg);
            
        case PLCRASH_X86_64_RBP:
            SETGEN(rbp, thread.uts.ts64, ts, regnum, reg);
            
        case PLCRASH_X86_64_RSP:
            SETGEN(rsp, thread.uts.ts64, ts, regnum, reg);
            
        case PLCRASH_X86_64_R8:
            SETGEN(r8, thread.uts.ts64, ts, regnum, reg);
            
        case PLCRASH_X86_64_R9:
            SETGEN(r9, thread.uts.ts64, ts, regnum, reg);
            
        case PLCRASH_X86_64_R10:
            SETGEN(r10, thread.uts.ts64, ts, regnum, reg);
            
        case PLCRASH_X86_64_R11:
            SETGEN(r11, thread.uts.ts64, ts, regnum, reg);
            
        case PLCRASH_X86_64_R12:
            SETGEN(r12, thread.uts.ts64, ts, regnum, reg);
            
        case PLCRASH_X86_64_R13:
            SETGEN(r13, thread.uts.ts64, ts, regnum, reg);
            
        case PLCRASH_X86_64_R14:
            SETGEN(r14, thread.uts.ts64, ts, regnum, reg);
            
        case PLCRASH_X86_64_R15:
            SETGEN(r15, thread.uts.ts64, ts, regnum, reg);
            
        case PLCRASH_X86_64_RIP:
            SETGEN(rip, thread.uts.ts64, ts, regnum, reg);
            
        case PLCRASH_X86_64_RFLAGS:
            SETGEN(rflags, thread.uts.ts64, ts, regnum, reg);
            
        case PLCRASH_X86_64_CS:
            SETGEN(cs, thread.uts.ts64, ts, regnum, reg);
            
        case PLCRASH_X86_64_FS:
            SETGEN(fs, thread.uts.ts64, ts, regnum, reg);
            
        case PLCRASH_X86_64_GS:
            SETGEN(gs, thread.uts.ts64, ts, regnum, reg);
            
        default:
            // Unsupported register
            __builtin_trap();
    }    
}

/**
 * @internal
 * 32-bit implementation of plcrash_async_thread_state_get_regname()
 */
static char const *plcrash_async_thread_state_get_regname_32 (plcrash_regnum_t regnum) {
    /* All word-sized registers */
    switch (regnum) {
        case PLCRASH_X86_EAX:
            return "eax";
            
        case PLCRASH_X86_EDX:
            return "edx";
            
        case PLCRASH_X86_ECX:
            return "ecx";
            
        case PLCRASH_X86_EBX:
            return "ebx";
            
        case PLCRASH_X86_EBP:
            return "ebp";
            
        case PLCRASH_X86_ESI:
            return "esi";
            
        case PLCRASH_X86_EDI:
            return "edi";
            
        case PLCRASH_X86_ESP:
            return "esp";
            
        case PLCRASH_X86_EIP:
            return "eip";
            
        case PLCRASH_X86_EFLAGS:
            return "eflags";
            
        case PLCRASH_X86_TRAPNO:
            return "trapno";
            
        case PLCRASH_X86_CS:
            return "cs";
            
        case PLCRASH_X86_DS:
            return "ds";
            
        case PLCRASH_X86_ES:
            return "es";
            
        case PLCRASH_X86_FS:
            return "fs";
            
        case PLCRASH_X86_GS:
            return "gs";
            
        default:
            // Unsupported register
            break;
    }
    
    /* Unsupported register is an implementation error (checked in unit tests) */
    PLCF_DEBUG("Missing register name for register id: %d", regnum);
    abort();
}

/**
 * @internal
 * 64-bit implementation of plcrash_async_thread_state_get_regname()
 */
static const char *plcrash_async_thread_state_get_regname_64 (plcrash_regnum_t regnum) {
    switch (regnum) {
        case PLCRASH_X86_64_RAX:
            return "rax";
            
        case PLCRASH_X86_64_RBX:
            return "rbx";
            
        case PLCRASH_X86_64_RCX:
            return "rcx";
            
        case PLCRASH_X86_64_RDX:
            return "rdx";
            
        case PLCRASH_X86_64_RDI:
            return "rdi";
            
        case PLCRASH_X86_64_RSI:
            return "rsi";
            
        case PLCRASH_X86_64_RBP:
            return "rbp";
            
        case PLCRASH_X86_64_RSP:
            return "rsp";
            
        case PLCRASH_X86_64_R8:
            return "r8";
            
        case PLCRASH_X86_64_R9:
            return "r9";
            
        case PLCRASH_X86_64_R10:
            return "r10";
            
        case PLCRASH_X86_64_R11:
            return "r11";
            
        case PLCRASH_X86_64_R12:
            return "r12";
            
        case PLCRASH_X86_64_R13:
            return "r13";
            
        case PLCRASH_X86_64_R14:
            return "r14";
            
        case PLCRASH_X86_64_R15:
            return "r15";
            
        case PLCRASH_X86_64_RIP:
            return "rip";
            
        case PLCRASH_X86_64_RFLAGS:
            return "rflags";
            
        case PLCRASH_X86_64_CS:
            return "cs";
            
        case PLCRASH_X86_64_FS:
            return "fs";
            
        case PLCRASH_X86_64_GS:
            return "gs";
            
        default:
            // Unsupported register
            break;
    }
    
    /* Unsupported register is an implementation error (checked in unit tests) */
    PLCF_DEBUG("Missing register name for register id: %d", regnum);
    abort();
}


/**
 * @internal
 * 32-bit implementation of plcrash_async_thread_state_map_dwarf_reg()
 */
plcrash_regnum_t plcrash_async_thread_state_map_dwarf_reg_32 (uint64_t dwarf_reg) {
#define MAPREG(_pl_num, _dwarf_num) do { \
    case _dwarf_num:\
        return _pl_num; \
} while (0)
    
    /*
     * DWARF register mappings as defined by GCC and LLVM/clang. These mappings
     * appear to have originally been defined by the SVR4 reference port C compiler,
     * and then later implemented by GCC for its 80386 target.
     *
     * This set of registers defines the common intersection of the gcc/llvm implementations.
     * It appears that LLVM implements a strict subset of the GCC-defined register set, but
     * further investigation is warranted prior to expanding the set of defined DWARF registers.
     *
     * Note that not all registers defined by gcc/LLVM are currently supported by our
     * thread-state API, and are not mapped.
     */
    switch (dwarf_reg) {
        MAPREG(PLCRASH_X86_EAX, 0);
        MAPREG(PLCRASH_X86_ECX, 1);
        MAPREG(PLCRASH_X86_EDX, 2);
        MAPREG(PLCRASH_X86_EBX, 3);
        MAPREG(PLCRASH_X86_ESP, 4);
        MAPREG(PLCRASH_X86_EBP, 5);
        MAPREG(PLCRASH_X86_ESI, 6);
        MAPREG(PLCRASH_X86_EDI, 7);
        MAPREG(PLCRASH_X86_EIP, 8);
    }
    
#undef MAPREG
    
    /* Unknown DWARF register.  */
    return PLCRASH_REG_INVALID;
}

/**
 * @internal
 * 64-bit implementation of plcrash_async_thread_state_map_dwarf_reg()
 */
plcrash_regnum_t plcrash_async_thread_state_map_dwarf_reg_64 (uint64_t dwarf_reg) {
#define MAPREG(_pl_num, _dwarf_num) do { \
    case _dwarf_num:\
        return _pl_num; \
} while (0)
    
    /*
     * DWARF register mappings as defined in the System V Application Binary Interface,
     * AMD64 Architecture Processor Supplement - Draft Version 0.99.6
     *
     * Note that not all registers defined the AMD64 ABI are currently supported by our
     * thread-state API, and are not mapped.
     */
    switch (dwarf_reg) {
        MAPREG(PLCRASH_X86_64_RAX, 0);
        MAPREG(PLCRASH_X86_64_RDX, 1);
        MAPREG(PLCRASH_X86_64_RCX, 2);
        MAPREG(PLCRASH_X86_64_RBX, 3);
        MAPREG(PLCRASH_X86_64_RSI, 4);
        MAPREG(PLCRASH_X86_64_RDI, 5);
        MAPREG(PLCRASH_X86_64_RBP, 6);
        MAPREG(PLCRASH_X86_64_RSP, 7);
        
        MAPREG(PLCRASH_X86_64_R8, 8);
        MAPREG(PLCRASH_X86_64_R9, 9);
        MAPREG(PLCRASH_X86_64_R10, 10);
        MAPREG(PLCRASH_X86_64_R11, 11);
        MAPREG(PLCRASH_X86_64_R12, 12);
        MAPREG(PLCRASH_X86_64_R13, 13);
        MAPREG(PLCRASH_X86_64_R14, 14);
        MAPREG(PLCRASH_X86_64_R15, 15);
        
        MAPREG(PLCRASH_X86_64_RFLAGS, 49);
        
        MAPREG(PLCRASH_X86_64_CS, 51);
        MAPREG(PLCRASH_X86_64_FS, 54);
        MAPREG(PLCRASH_X86_64_GS, 55);
    }
    
#undef MAPREG
    
    /* Unknown DWARF register.  */
    return PLCRASH_REG_INVALID;
}

#endif /* defined(__i386__) || defined(__x86_64__) */
