
.text
.align 4
.globl thlet_switch_to
.type thlet_switch_to, @function
.global thlet_switch_to

thlet_switch_to:
    pushq	%rbp
	pushq	%rbx
	pushq	%r12
	pushq	%r13
	pushq	%r14
	pushq	%r15

	movq	%rsp, %rsp
	movq	%rsp, %rsp

	popq	%r15
	popq	%r14
	popq	%r13
	popq	%r12
	popq	%rbx
	popq	%rbp
    ret


.text
.align 4
.globl thlet_switch_to_test
.type thlet_switch_to_test, @function
.global thlet_switch_to_test

thlet_switch_to_test:
	pushq	%r15
    pushq	%rbp
	pushq	%rbx
	pushq	%r12
	pushq	%r13
	pushq	%r14

	movq	%r15, %r15
	movq	%r15, %r15

	popq	%r15
	popq	%r15
	popq	%r15
	popq	%r15
	popq	%r15
	popq	%r15
    ret