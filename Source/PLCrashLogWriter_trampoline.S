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

#include "PLCrashLogWriter_trampoline_private.h"


/*
 *
 * plcrash_error_t plcrash_log_writer_write_curthread (plcrash_log_writer_t *writer,
 *                                                     plcrash_async_image_list_t *image_list,
 *                                                     plcrash_async_file_t *file,
 *                                                     siginfo_t *siginfo);
 */

#if __arm__
.align 2
.arm
#endif

.text
.globl _plcrash_log_writer_write_curthread
_plcrash_log_writer_write_curthread:

#if __x86_64__

pushq   %rbp
movq    %rsp, %rbp
subq    $720, %rsp // Size of 712 + 8 bytes for required alignment

#define MOVQ(reg, offset) movq %##reg, offset(%rsp)

// These assumed offsets are compile-time validated in plframe_context.m, and are ABI-stable.
MOVQ    (rax,   16)
MOVQ    (rbx,   24);
MOVQ    (rcx,   32);
MOVQ    (rdx,   40);
MOVQ    (rdi,   48);
MOVQ    (rsi,   56);

/* ->rbp: Use our saved copy of the caller's frame pointer */
movq    (%rbp), %rcx
movq    %rcx, 64(%rsp)

/* ->rsp: Use our saved copy of the caller's stack pointer. */
MOVQ    (rbp,   72);

MOVQ    (r8,    80);
MOVQ    (r9,    88);
MOVQ    (r10,   96);
MOVQ    (r11,   104);
MOVQ    (r12,   112);
MOVQ    (r13,   120);
MOVQ    (r14,   128);
MOVQ    (r15,   136);

/* Use the return address for our RIP value */
movq    0x8(%rbp), %rcx
movq    %rcx, 144(%rsp)

pushfq
popq    %rcx
movq    %rcx, 152(%rsp)

MOVQ    (cs, 160);
MOVQ    (fs, 168);
MOVQ    (gs, 176);

/* Restore the 4th argument value */
movq    32(%rsp), %rcx

/* Move mctx to 5th argument of plcrash_log_writer_write_curthread_stub */
movq    %rsp, %r8

/* Move stack pointer to 6th argument of plcrash_log_writer_write_curthread_stub */
movq    %rbp, %r9

xorb	%al, %al
callq    _plcrash_log_writer_write_curthread_stub

addq    $720, %rsp
popq    %rbp
ret

#elif __i386__

xorl    %eax, %eax
ret

#elif defined(__arm__)

// TODO

#else

#error Unsupported Platform

#endif