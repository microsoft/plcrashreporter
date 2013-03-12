#if defined(__x86_64__)
//  This triggers common encoding handling with _main
.globl _main_common
_main_common:
	.cfi_startproc
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp

	movl	$5, %eax
	popq	%rbp
	ret
	.cfi_endproc

// This hack forces the use of a common encoding
// by preventing the use of an entry range that includes
// both _main_common and _main
.globl _main_noncommon
_main_noncommon:
	.cfi_startproc
	.cfi_def_cfa_offset 32
	nop
	ret
	.cfi_endproc

.globl _main
_main:
	.cfi_startproc
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp

	movl	$5, %eax
	popq	%rbp
	ret
	.cfi_endproc
#elif defined(__i386__)
.globl _main
_main:
	.cfi_startproc
	// TODO
	movl	$5, %eax
	ret
	.cfi_endproc
#elif defined(__arm__)
.align 4
.arm
.globl _main
_main:
	// TODO
	mov	r0, #5
	pop	{pc}
#endif
