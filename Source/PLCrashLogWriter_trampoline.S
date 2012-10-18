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
.align 4
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

// These assumed offsets are compile-time validated in PLCrashLogWriter_trampoline.m, and are ABI-stable.
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

xorb	%al, %al
callq    _plcrash_log_writer_write_curthread_stub

addq    $720, %rsp
popq    %rbp
ret

#elif __i386__

pushl   %ebp
movl    %esp, %ebp
subl    $632, %esp // Size of 600 for context + 12 bytes for required alignment + 20 bytes for call arguments

#define OFF 32
#define MOVL(reg, offset) movl %##reg, OFF+offset(%esp)

// These assumed offsets are compile-time validated in PLCrashLogWriter_trampoline.m, and are ABI-stable.

/* es */
movl    $0, 0+OFF(%esp) // trapno

/* ss */
MOVL    (eax,   12)
MOVL    (ebx,   16);
MOVL    (ecx,   20);
MOVL    (edx,   24);
MOVL    (edi,   28);
MOVL    (esi,   32);
MOVL    (ebp,   36);
MOVL    (esp,   40);

/* ss.eflags */
pushf
pop     %eax
movl    %eax, 48+OFF(%esp)

/* Use the return address for our RIP value */
movl    0x4(%ebp), %eax
movl    %eax, 52+OFF(%esp)

MOVL    (cs, 56);
MOVL    (ds, 60);
MOVL    (es, 64);
MOVL    (fs, 68);
MOVL    (gs, 72);
#undef MOVL

/* Set up our argument stack: writer (arg0), image_list, file, siginfo, mctx */
movl    8(%ebp), %eax   // arg0 - writer
movl    %eax,  (%esp)

movl    12(%ebp), %eax   // arg1 - image_list
movl    %eax, 4(%esp)

movl    16(%ebp), %eax  // arg2 - file
movl    %eax, 8(%esp)

movl    20(%ebp), %eax  // arg3 - siginfo
movl    %eax, 12(%esp)

movl    %esp, %eax      // arg4 - mctx
addl    $OFF, %eax
movl    %eax, 16(%esp)

call    _plcrash_log_writer_write_curthread_stub

addl    $632, %esp
popl    %ebp
ret

#elif defined(__arm__)


push    {r7, lr}
mov     r7, sp
sub     sp, sp, #344 // Size of 340 for context + 4 bytes for call argument

// These assumed offsets are compile-time validated in PLCrashLogWriter_trampoline.m, and are ABI-stable.

/* Save r0 before we stomp it. The destination address is our SP + 4 (call argument) + 12 (offset to r[] array) */
str     r0, [sp, #16]

/* Calculate the mcontext_t pointer and store it on the stack, in preparation for our call below */
add     r0, sp, #4
str     r0, [sp]

/* Write out GP registers. The offset is r[1]. */
add     r0, r0, #16
stmia   r0, {r1-r12}

/* Fetch our caller's frame pointer */
ldr     r2, [r7]

/* Overwrite r[7] with the caller's fp value */
str     r2, [r0, #24]

/* User the caller's SP value */
add     r1, r7, #8    // account for the 2 byte push in prologue
str     r1, [r0, #48] // 64 - 16 byte offset to r[1]

/* Fetch the link register from our caller's frame */
ldr     r1, [r2, #4]
str     r1, [r0, #52] // 68 - 16 byte offset to r[1]

/* Use the return address for our PC value */
ldr     r1, [r7, #4]
str     r1, [r0, #56] // 72 - 16 byte offset to r[1]

/* Fetch CPSR */
mrs     r1, cpsr
str     r1, [r0, #60] // 76 - 16 byte offset to r[1]

/* Restore the r0-r2 argument values. The source address is our SP + 4 (call argument) + 12 (offset to r[] array). */
ldr     r0, [sp, #16];
ldr     r1, [sp, #20];
ldr     r2, [sp, #24];

bl      _plcrash_log_writer_write_curthread_stub
mov     sp, r7
pop     {r7, pc}

#else

#error Unsupported Platform

#endif