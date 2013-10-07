/*
 Copyright (c) 2008-2011 Apple Inc. All rights reserved.
 Copyright (c) 2013 Plausible Labs, Inc. All rights reserved.
 Rewritten for arm64 by Plausible Labs

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

.data
.globl _unwind_tester_list_arm64_frame
_unwind_tester_list_arm64_frame:
.quad   _unwind_test_arm64_frame_no_reg
.quad   _unwind_test_arm64_frame_x19_x20
.quad	0


.text
.align 2
.globl _unwind_test_arm64_frame_no_reg
_unwind_test_arm64_frame_no_reg:
    stp	fp, lr, [sp, #-16]!
LT1_stp:
    mov	fp, sp
LT1_fp:
    bl _uwind_to_main
    mov	sp, fp
    ldp	fp, lr, [sp], #16
    ret	lr
LT1_end:

.globl _unwind_test_arm64_frame_x19_x20
_unwind_test_arm64_frame_x19_x20:
    stp	fp, lr, [sp, #-16]!
LT2_stp:
    mov	fp, sp
LT2_fp:
    stp	x20, x19, [sp, #-16]!
LT2_pushed_regs:
    mov x19, xzr
    mov x20, xzr
    bl _uwind_to_main
    mov	sp, fp
    ldp     x20, x19, [sp], #16
    ldp	fp, lr, [sp], #16
    ret	lr
LT2_end:

.section __TEXT,__eh_frame,coalesced,no_toc+strip_static_syms+live_support
EH_frame1:
.set L$set$0,LECIE1-LSCIE1
.long L$set$0 ; Length of Common Information Entry
LSCIE1:
.long	0x0 ; CIE Identifier Tag
.byte	0x1 ; CIE Version
.ascii "zR\0" ; CIE Augmentation
.byte	0x1 ; uleb128 0x1; CIE Code Alignment Factor
.byte	0x78 ; sleb128 -8; CIE Data Alignment Factor
.byte	0x1E ; CIE RA Column
.byte	0x1 ; uleb128 0x1; Augmentation size
.byte	0x10 ; FDE Encoding (pcrel)
.byte	0xc ; DW_CFA_def_cfa
.byte	0x1F ; uleb128 31 (x31)
.byte	0x0 ; uleb128 0x0
.align 3
LECIE1:

.globl _unwind_test_arm64_frame_no_reg.eh
_unwind_test_arm64_frame_no_reg.eh:
LSFDE1:
.set L$set$5,LEFDE1-LASFDE1
.long L$set$5 ; FDE Length
LASFDE1:
.long	LASFDE1-EH_frame1 ; FDE CIE offset
.quad	_unwind_test_arm64_frame_no_reg-. ; FDE initial location
.set L$set$6,LT1_end-_unwind_test_arm64_frame_no_reg
.quad L$set$6 ; FDE address range
.byte	0x0 ; uleb128 0x0; Augmentation size
.byte	0x4 ; DW_CFA_advance_loc4
.set L$set$7,LT1_stp-_unwind_test_arm64_frame_no_reg
.long L$set$7
.byte	0xe ; DW_CFA_def_cfa_offset
.byte	0x10 ; uleb128 0x10
.byte	0x9D ; DW_CFA_offset, column 0x1D
.byte	0x2 ; uleb128 0x2
.byte	0x9E ; DW_CFA_offset, column 0x1E
.byte	0x1	; uleb128 0x1
.byte	0x4 ; DW_CFA_advance_loc4
.set L$set$8,LT1_fp-LT1_stp
.long L$set$8
.byte	0xd ; DW_CFA_def_cfa_register
.byte	0x1D ; uleb128 0x1D
.align 3
LEFDE1:

.globl _unwind_test_arm64_frame_x19_x20.eh
_unwind_test_arm64_frame_x19_x20.eh:
LSFDE2:
.set L$set$10,LEFDE2-LASFDE2
.long L$set$10 ; FDE Length
LASFDE2:
.long	LASFDE2-EH_frame1 ; FDE CIE offset
.quad	_unwind_test_arm64_frame_x19_x20-. ; FDE initial location
.set L$set$11,LT2_end-_unwind_test_arm64_frame_x19_x20
.quad L$set$11 ; FDE address range
.byte	0x0 ; uleb128 0x0; Augmentation size
.byte	0x4 ; DW_CFA_advance_loc4
.set L$set$12,LT2_stp-_unwind_test_arm64_frame_x19_x20
.long L$set$12
.byte	0xe ; DW_CFA_def_cfa_offset
.byte	0x10 ; uleb128 0x10
.byte	0x9D ; DW_CFA_offset, column 0x1D
.byte	0x2 ; uleb128 0x2
.byte	0x9E ; DW_CFA_offset, column 0x1E
.byte	0x1	; uleb128 0x1
.byte	0x4 ; DW_CFA_advance_loc4
.set L$set$13,LT2_fp-LT2_stp
.long L$set$13
.byte	0xd ; DW_CFA_def_cfa_register
.byte	0x1D ; uleb128 0x1D
.byte	0x4 ; DW_CFA_advance_loc4
.set L$set$14,LT2_pushed_regs-LT2_fp
.long L$set$14
.byte	0x93	; DW_CFA_offset, column 0x13
.byte	0x3	; uleb128 0x3
.byte	0x94	; DW_CFA_offset, column 0x14
.byte	0x4	; uleb128 0x4
.align 3
LEFDE2:

#endif /* __arm64__ */
