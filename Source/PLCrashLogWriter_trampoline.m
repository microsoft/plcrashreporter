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

#import <mach/mach.h>
#import <stddef.h>

#import "PLCrashLogWriter.h"
#import "PLCrashFrameWalker.h"
#import "PLCrashLogWriter_trampoline_private.h"


/*
 * Implements the interior function called by plcrash_log_writer_write_curthread()
 * after it has populated the mctx thread state.
 */
plcrash_error_t plcrash_log_writer_write_curthread_stub (plcrash_log_writer_t *writer,
                                                         plcrash_async_image_list_t *image_list,
                                                         plcrash_async_file_t *file, 
                                                         siginfo_t *siginfo,
                                                         _STRUCT_MCONTEXT *mctx)
{
    _STRUCT_UCONTEXT ctx;

    ctx.uc_onstack = 0;
    ctx.uc_stack.ss_sp = pthread_get_stackaddr_np(pthread_self());
    ctx.uc_stack.ss_flags = 0;
    ctx.uc_stack.ss_size = pthread_get_stacksize_np(pthread_self());

    sigprocmask(0, NULL, &ctx.uc_sigmask);

    ctx.uc_mcsize = PL_MCONTEXT_SIZE;
    ctx.uc_mcontext = mctx;
    
    /* Zero unsupported thread states */
    memset(&mctx->__es, 0, sizeof(mctx->__es));
    memset(&mctx->__fs, 0, sizeof(mctx->__fs));

    return plcrash_log_writer_write(writer, image_list, file, siginfo, &ctx);
}


/*
 * Compile time structure sanity checking of our assumed
 * structure layouts. The layouts are ABI-stable.
 */

/* Provides compile-time validation */
#define VALIDATE_2(name, cond, line)    typedef int cc_validate_##name##line [(cond) ? 1 : -1]
#define VALIDATE_1(name, cond, line)    VALIDATE_2(name, cond, line)
#define VALIDATE(name, cond)            VALIDATE_1(name, cond, __LINE__)

/* 
 * Validate non-architecture specific constraints
 */

/* sizeof(struct mcontext) */
VALIDATE(MCONTEXT_SIZE, sizeof(_STRUCT_MCONTEXT) == PL_MCONTEXT_SIZE);

/* sizeof(struct ucontext) */
VALIDATE(UCONTEXT_SIZE, sizeof(_STRUCT_UCONTEXT) == PL_UCONTEXT_SIZE);


#if __x86_64__

/* There's a hard-coded dependency on this size in the trampoline assembly, so we explicitly validate it here. */
VALIDATE(MCONTEXT_SIZE, sizeof(_STRUCT_MCONTEXT) == 712);

/* Verify the expected offsets */
#define OFF(reg, offset) (offsetof(_STRUCT_MCONTEXT, __ss.__##reg) == offset)
#define VOFF(reg, offset) VALIDATE(MCONTEXT_SS_OFFSET_##reg##_, OFF(reg, offset))

VOFF(rax, 16);
VOFF(rbx, 24);
VOFF(rcx, 32);
VOFF(rdx, 40);
VOFF(rdi, 48);
VOFF(rsi, 56);
VOFF(rbp, 64);
VOFF(rsp, 72);
VOFF(r8, 80);
VOFF(r9, 88);
VOFF(r10, 96);
VOFF(r11, 104);
VOFF(r12, 112);
VOFF(r13, 120);
VOFF(r14, 128);
VOFF(r15, 136);
VOFF(rip, 144);
VOFF(rflags, 152);
VOFF(cs, 160);
VOFF(fs, 168);
VOFF(gs, 176);

#undef OFF
#undef VOFF

#elif __i386__

/* There's a hard-coded dependency on this size in the trampoline assembly, so we explicitly validate it here. */
VALIDATE(MCONTEXT_SIZE, sizeof(_STRUCT_MCONTEXT) == 600);

/* Verify the expected offsets */
#define OFF(struct, reg, offset) (offsetof(_STRUCT_MCONTEXT, __##struct.__##reg) == offset)
#define VOFF(struct, reg, offset) VALIDATE(MCONTEXT_SS_OFFSET_##reg##_, OFF(struct, reg, offset))

VOFF(ss, eax, 12);
VOFF(ss, ebx, 16);
VOFF(ss, ecx, 20);
VOFF(ss, edx, 24);
VOFF(ss, edi, 28);
VOFF(ss, esi, 32);
VOFF(ss, ebp, 36);
VOFF(ss, esp, 40);
// ss
VOFF(ss, eflags, 48);
VOFF(ss, eip, 52);
VOFF(ss, cs, 56);
VOFF(ss, ds, 60);
VOFF(ss, es, 64);
VOFF(ss, fs, 68);
VOFF(ss, gs, 72);

VOFF(es, trapno, 0);

#undef OFF
#undef VOFF

#elif defined(__arm__)

/* There's a hard-coded dependency on this size in the trampoline assembly, so we explicitly validate it here. */
VALIDATE(MCONTEXT_SIZE, sizeof(_STRUCT_MCONTEXT) == 340);

/* Verify the expected offsets */
#define OFF(struct, reg, offset) (offsetof(_STRUCT_MCONTEXT, __##struct.__##reg) == offset)
#define VOFF(struct, reg, offset) VALIDATE(MCONTEXT_SS_OFFSET_##reg##_, OFF(struct, reg, offset))


VOFF(ss, r, 12);
VOFF(ss, sp, 64);
VOFF(ss, lr, 68);
VOFF(ss, pc, 72);
VOFF(ss, cpsr, 76);

#undef OFF
#undef VOFF

#else

#error Unimplemented on this architecture

#endif
