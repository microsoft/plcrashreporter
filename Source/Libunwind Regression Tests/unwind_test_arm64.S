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

.data
.globl _unwind_tester_target_ip ; IP that we'll unwind to
_unwind_tester_target_ip:
.quad	Ltester_target_ip

;
; This is a generic function to test that restoring registers during unwinding work correctly.
;
.text
.globl _unwind_tester
_unwind_tester:
.cfi_startproc
stp	fp, lr, [sp, #-16]!
mov	fp, sp
stp	x20, x19, [sp, #-16]!
stp	x22, x21, [sp, #-16]!
stp	x24, x23, [sp, #-16]!
stp	x26, x25, [sp, #-16]!
stp	x28, x27, [sp, #-16]!
.cfi_def_cfa    w29, 16
.cfi_offset     w30, -8
.cfi_offset     w29, -16
.cfi_offset     w19, -24
.cfi_offset     w20, -32
.cfi_offset     w21, -40
.cfi_offset     w22, -48
.cfi_offset     w23, -56
.cfi_offset     w24, -64
.cfi_offset     w25, -72
.cfi_offset     w26, -80
.cfi_offset     w27, -88
.cfi_offset     w28, -96

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
Ltester_target_ip: ; IP that we'll unwind to

; verify that non-volatile registers still contain magic values
.macro cmp64
lit64 x1, $1, $2, $3, $4
cmp x1, $0
b.ne L2
.endmacro
cmp64   x19, 0x1234, 0x5678, 0x8765, 0x4321
cmp64   x20, 0x0246, 0x8ACE, 0xECA8, 0x6420
cmp64   x21, 0x1357, 0x9BDF, 0xFDB9, 0x7531
cmp64   x22, 0x1122, 0x3344, 0x5566, 0x7788
cmp64   x23, 0x0022, 0x4466, 0x88AA, 0xCCEE

cmp64   x24, 0x0033, 0x5577, 0x99BB, 0xDDFF
cmp64   x25, 0x0044, 0x6688, 0xAACC, 0xEE00
cmp64   x26, 0x0066, 0x88AA, 0xCCEE, 0xFF11
cmp64   x27, 0x0088, 0xAACC, 0xEEFF, 0x1133
cmp64   x28, 0xCAFE, 0xDEAD, 0xF00D, 0xBEEF

mov     x0, #0
b		L3
L2:
mov     x0, #1
L3:
ldp     x28, x27, [sp], #16
ldp     x26, x25, [sp], #16
ldp     x24, x23, [sp], #16
ldp     x22, x21, [sp], #16
ldp     x20, x19, [sp], #16
ldp     fp, lr, [sp], #16
ret
Lunwind_tester_end:
.cfi_endproc

.subsections_via_symbols
.section	__LD,__compact_unwind,regular,debug
.align	3
.quad	(_unwind_tester) ; Range Start
Ltmp_range = (Lunwind_tester_end-_unwind_tester)
.long	Ltmp_range ; Range Length
.long	0x0400001F      ; Compact Unwind Encoding
.quad	0               ; Personality Function
.quad	0               ; LSDA

#endif /* __arm64__ */
