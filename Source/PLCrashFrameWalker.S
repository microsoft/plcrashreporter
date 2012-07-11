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

#define PLFRAME_ESUCCESS 0

/* Enable thumb on ARMv7 */
#if __arm__
#  ifdef _ARM_ARCH_7
.thumb
.thumb_func _plframe_getcontext
#  else
.arm
#  endif
#endif

.text
.globl _plframe_getcontext
_plframe_getcontext:

#if __x86_64__

    // TODO
    xorl %eax, %eax
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