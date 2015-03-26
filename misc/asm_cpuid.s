	.section	__TEXT,__text,regular,pure_instructions
	.globl	_main
	.align	4, 0x90
_main:                                  ## @main
	.cfi_startproc
## BB#0:
	pushq	%rbp
Ltmp2:
	.cfi_def_cfa_offset 16
Ltmp3:
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
Ltmp4:
	.cfi_def_cfa_register %rbp
	subq	$32, %rsp
	leaq	-12(%rbp), %rsi
	leaq	-16(%rbp), %rdx
	leaq	-20(%rbp), %rcx
	leaq	-24(%rbp), %r8
	movl	$0, -4(%rbp)
	movl	$0, -8(%rbp)
	movl	-8(%rbp), %edi
	callq	_cpuid
	leaq	L_.str(%rip), %rdi
	movl	-8(%rbp), %esi
	movl	-12(%rbp), %edx
	movb	$0, %al
	callq	_printf
	movl	$0, %edx
	movl	%eax, -28(%rbp)         ## 4-byte Spill
	movl	%edx, %eax
	addq	$32, %rsp
	popq	%rbp
	retq
	.cfi_endproc

	.align	4, 0x90
_cpuid:                                 ## @cpuid
	.cfi_startproc
## BB#0:
	pushq	%rbp
Ltmp8:
	.cfi_def_cfa_offset 16
Ltmp9:
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
Ltmp10:
	.cfi_def_cfa_register %rbp
	pushq	%rbx
Ltmp11:
	.cfi_offset %rbx, -24
	movl	%edi, -12(%rbp)
	movq	%rsi, -24(%rbp)
	movq	%rdx, -32(%rbp)
	movq	%rcx, -40(%rbp)
	movq	%r8, -48(%rbp)
	movl	-12(%rbp), %eax
	## InlineAsm Start
	cpuid
	## InlineAsm End
	movl	%eax, -52(%rbp)
	movl	%ebx, -56(%rbp)
	movl	%ecx, -60(%rbp)
	movl	%edx, -64(%rbp)
	cmpq	$0, -24(%rbp)
	je	LBB1_2
## BB#1:
	movl	-52(%rbp), %eax
	movq	-24(%rbp), %rcx
	movl	%eax, (%rcx)
LBB1_2:
	cmpq	$0, -32(%rbp)
	je	LBB1_4
## BB#3:
	movl	-56(%rbp), %eax
	movq	-32(%rbp), %rcx
	movl	%eax, (%rcx)
LBB1_4:
	cmpq	$0, -40(%rbp)
	je	LBB1_6
## BB#5:
	movl	-60(%rbp), %eax
	movq	-40(%rbp), %rcx
	movl	%eax, (%rcx)
LBB1_6:
	cmpq	$0, -48(%rbp)
	je	LBB1_8
## BB#7:
	movl	-64(%rbp), %eax
	movq	-48(%rbp), %rcx
	movl	%eax, (%rcx)
LBB1_8:
	popq	%rbx
	popq	%rbp
	retq
	.cfi_endproc

	.section	__TEXT,__cstring,cstring_literals
L_.str:                                 ## @.str
	.asciz	"%u %x\n"


.subsections_via_symbols
