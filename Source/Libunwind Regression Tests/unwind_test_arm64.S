/*
 Copyright (c) 2008-2011 Apple Inc. All rights reserved.

 Rewritten for arm64 by Plausible Labs
 Copyright (c) 2013 Plausible Labs, Inc. All rights reserved.

 This file contains Original Code and/or Modifications of Original Code
 as defined in and that are subject to the Apple Public Source License
 Version 2.0 (the 'License'). You may not use this file except in
 compliance with the License. Please obtain a copy of the License at
 http://www.opensource.apple.com/apsl/ and read it before using this
 file.
 
 The Original Code and all software distributed under the License are
 distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 Please see the License for the specific language governing rights and
 limitations under the License.
 */

#ifdef __arm64__

;
; This is a generic function to test that restoring registers during unwinding work correctly.
;

.text
.globl _unwind_tester
_unwind_tester:
.cfi_startproc
LFB3:
stp     fp, lr, [sp, #-16]!
LCFI45:
mov     fp, sp
LCFI46:
stp     x19, x20, [sp, #-80]
LCFI47:
stp     x21, x22, [sp, #-64]
LCFI48:
stp     x23, x24, [sp, #-48]
LCFI49:
stp     x25, x26, [sp, #-32]
LCFI50:
stp     x27, x28, [sp, #-16]
LCFI51:
sub     sp, sp, #80
LCFI52:

.macro lit64
movz    $0, #$1, lsl #48
movk    $0, #$2, lsl #32
movk    $0, #$3, lsl #16
movk    $0, #$4
.endmacro

lit64   x19, 0x1234, 0x5678, 0x8765, 0x4321
lit64   x20, 0x0246, 0x8ACE, 0xECA8, 0x6420
lit64   x21, 0x1357, 0x9BDF, 0xFDB9, 0x7531
lit64   x22, 0x1122, 0x3344, 0x5566, 0x7788
lit64   x23, 0x0022, 0x4466, 0x88AA, 0xCCEE

lit64   x24, 0x0033, 0x5577, 0x99BB, 0xDDFF
lit64   x25, 0x0044, 0x6688, 0xAACC, 0xEE00
lit64   x26, 0x0066, 0x88AA, 0xCCEE, 0xFF11
lit64   x27, 0x0088, 0xAACC, 0xEEFF, 0x1133
lit64   x28, 0xCAFE, 0xDEAD, 0xF00D, 0xBEEF

; save the SP value for use by our tests.
mov     x2, sp
str     x2, [x1]
; call test function which will invoke unwinder which "returns" here
blr     x0
.globl _unwind_tester_target_ip ; IP that we'll unwind to
_unwind_tester_target_ip:
#if 0
// TODO
# verify that non-volatile registers still contain magic values
movq	$0x1234567887654321, %rax
cmpq	%rax, %rbx
jne	L2
movq	$0x02468ACEECA86420, %rax
cmpq	%rax, %r12
jne	L2
movq	$0x13579BDFFDB97531, %rax
cmpq	%rax, %r13
jne	L2
movq	$0x1122334455667788, %rax
cmpq	%rax, %r14
jne	L2
movq	$0x0022446688AACCEE, %rax
cmpq	%rax, %r15
jne	L2
#endif
mov     x0, #0
b		L3
L2:
mov     x0, #1
L3:
ldp     x19, x20, [fp, #-80]
ldp     x21, x22, [fp, #-64]
ldp     x23, x24, [fp, #-48]
ldp     x25, x26, [fp, #-32]
ldp     x27, x28, [fp, #-16]

mov     sp, fp
ldp     fp, lr, [sp], #16
ret     lr
LFE3:
.cfi_endproc

#if 1
.section __TEXT,__eh_frame,coalesced,no_toc+strip_static_syms+live_support
EH_frame1:
.set L$set$0,LECIE1-LSCIE1
.long L$set$0	; Length of Common Information Entry
LSCIE1:
.long	0x0	; CIE Identifier Tag
.byte	0x1	; CIE Version
.ascii "zR\0"	; CIE Augmentation
.byte	0x1	; uleb128 0x1; CIE Code Alignment Factor
.byte	0x78	; sleb128 -8; CIE Data Alignment Factor
.byte	0x10	; CIE RA Column
.byte	0x1	; uleb128 0x1; Augmentation size
.byte	0x10	; FDE Encoding (pcrel)
.byte	0xc	; DW_CFA_def_cfa
.byte	0x1F	; uleb128 31 (x31)
.byte	0x8	; uleb128 0x8
.byte	0x90	; DW_CFA_offset, column 0x10
.byte	0x1	; uleb128 0x1
.align 3
LECIE1:

// TODO - Offsets are likely wrong, but register numbers should be correct
.globl _unwind_tester.eh
_unwind_tester.eh:
LSFDE23:
.set L$set$52,LEFDE23-LASFDE23
.long L$set$52	; FDE Length
LASFDE23:
.long	LASFDE23-EH_frame1	; FDE CIE offset
.quad	LFB3-.	; FDE initial location
.set L$set$53,LFE3-LFB3
.quad L$set$53	; FDE address range
.byte	0x0	; uleb128 0x0; Augmentation size
.byte	0x4	; DW_CFA_advance_loc4
.set L$set$54,LCFI45-LFB3
.long L$set$54
.byte	0xe	; DW_CFA_def_cfa_offset
.byte	0x10	; uleb128 0x10
.byte	0x9D	; DW_CFA_offset, column 0x1D (x29)
.byte	0x2	; uleb128 0x2
.byte	0x4	; DW_CFA_advance_loc4
.set L$set$55,LCFI46-LCFI45
.long L$set$55
.byte	0xd	; DW_CFA_def_cfa_register
.byte	0x6	; uleb128 0x1D (x29)
.byte	0x4	; DW_CFA_advance_loc4
.set L$set$56,LCFI52-LCFI46
.long L$set$56
.byte	0x93	; DW_CFA_offset, column 0x13
.byte	0x3	; uleb128 0x3
.byte	0x94	; DW_CFA_offset, column 0x14
.byte	0x4	; uleb128 0x4
.byte	0x95	; DW_CFA_offset, column 0x15
.byte	0x5	; uleb128 0x5
.byte	0x96	; DW_CFA_offset, column 0x16
.byte	0x6	; uleb128 0x6
.byte	0x97	; DW_CFA_offset, column 0x17
.byte	0x7	; uleb128 0x7
.byte	0x98	; DW_CFA_offset, column 0x18
.byte	0x8	; uleb128 0x3
.byte	0x99	; DW_CFA_offset, column 0x19
.byte	0x9	; uleb128 0x4
.byte	0x9A	; DW_CFA_offset, column 0x1A
.byte	0xA	; uleb128 0x5
.byte	0x9B	; DW_CFA_offset, column 0x1B
.byte	0xB	; uleb128 0x6
.byte	0x9C	; DW_CFA_offset, column 0x1C
.byte	0xC	; uleb128 0x7
.align 3
LEFDE23:
#endif

#endif /* __arm64__ */
