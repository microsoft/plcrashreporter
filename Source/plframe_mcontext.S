/*
 * Author: Landon Fuller <landonf@plausible.coop>
 *
 * Copyright (c) 2012 Plausible Labs Cooperative, Inc.
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

#include "plframe_mcontext_private.h"

/* Enable thumb on ARMv7 */
#if __arm__
.align 2
.arm
#endif

.text
.globl _plframe_getmcontext
_plframe_getmcontext:

#if __x86_64__


#define MOVQ(reg, offset) movq %##reg, offset(%rdi)

    // These assumed offsets are compile-time validated in plframe_context.m, and are ABI-stable.
    MOVQ(rax,   16)
    MOVQ(rbx,   24);
    MOVQ(rcx,   32);
    MOVQ(rdx,   40);
    MOVQ(rdi,   48);
    MOVQ(rsi,   56);
    MOVQ(rbp,   64);
    MOVQ(rsp,   72);
    MOVQ(r8,    80);
    MOVQ(r9,    88);
    MOVQ(r10,   96);
    MOVQ(r11,   104);
    MOVQ(r12,   112);
    MOVQ(r13,   120);
    MOVQ(r14,   128);
    MOVQ(r15,   136);

    /* Use the return address for our RIP value */
    movq (%rsp), %rcx
    movq %rcx, 144(%rdi)

    pushfq
    popq %rcx
    movq %rcx, 152(%rdi)

    MOVQ(cs, 160);
    MOVQ(fs, 168);
    MOVQ(gs, 176);

#undef MOVQ

    movl $PLFRAME_MCONTEXT_ESUCCESS, %eax
    ret

#elif __i386__

#define MOVL(reg, offset) movl %##reg, offset(%ecx)

    // mctx in first argument
    movl 0x4(%esp), %ecx 

    // These assumed offsets are compile-time validated in plframe_context.m, and are ABI-stable.

    // trapno not supported
    movl $0, 0(%ecx)

    MOVL(eax,   12)
    MOVL(ebx,   16);
    MOVL(ecx,   20);
    MOVL(edx,   24);
    MOVL(edi,   28);
    MOVL(esi,   32);
    MOVL(ebp,   36);
    MOVL(esp,   40);

    // eflags
    pushf
    pop %eax
    movl %eax, 48(%ecx)

    /* Use the return address for our RIP value */
    movl (%esp), %eax
    movl %eax, 52(%ecx)

    MOVL(cs, 56);
    MOVL(ds, 60);
    MOVL(es, 64);
    MOVL(fs, 68);
    MOVL(gs, 72);
#undef MOVL

    xorl %eax, %eax
    ret

#elif __powerpc__

#error Unimplemented on PPC

#elif defined(__arm__)
    // These assumed offsets are compile-time validated in plframe_context.m, and are ABI-stable.

    /* Calculate offset to r4. */ 
    add r1, r0, #24

    /* Write out our GP registers. We don't need to save r0-r3 */
    stmia r1, {r4-r13}

    /* Fetch the link register from our caller's frame pointer */
    ldr r1, [r7, #4]
    str r1, [r0, #68]

    /* Use the return address for our PC value */
    str lr, [r0, #72]

    mov r0, #PLFRAME_MCONTEXT_ESUCCESS
    bx lr
 
#else

#error Unimplemented

#endif