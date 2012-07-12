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
#  include <arm/arch.h>
#  ifdef _ARM_ARCH_7
.thumb
.thumb_func _plframe_getmcontext
#  else
.arm
#  endif
#endif

.text
.globl _plframe_getmcontext
_plframe_getmcontext:

#if __x86_64__

#if TEST_MODE
#  define MOVQ(reg, offset) movq %##reg, offset(%rdi)
#else
#  define MOVQ(reg, offset) movq %##reg, offset(%rdi)
#endif

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

    // TODO
    xorl %eax, %eax
    ret

#elif __powerpc__

#error Unimplemented on PPC

#elif defined(__arm__)

    // TODO
    bx lr
 
#else

#error Unimplemented

#endif