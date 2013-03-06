#if defined(__x86_64__)
.globl _main
_main:
	.cfi_startproc
	// TODO
	movl	$5, %eax
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
