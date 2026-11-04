# chimera_ctx_switch(void** save_sp, void* load_sp) - see chimera_context.h.
# Nothing is kept below the stack pointer (a sandboxed guest may not).
	.text
	.globl chimera_ctx_switch
	.type chimera_ctx_switch, @function
chimera_ctx_switch:
	pushq %rbp
	pushq %rbx
	pushq %r12
	pushq %r13
	pushq %r14
	pushq %r15
	subq $8, %rsp
	stmxcsr (%rsp)
	fnstcw 4(%rsp)
	movq %rsp, (%rdi)
	movq %rsi, %rsp
	ldmxcsr (%rsp)
	fldcw 4(%rsp)
	addq $8, %rsp
	popq %r15
	popq %r14
	popq %r13
	popq %r12
	popq %rbx
	popq %rbp
	ret
	.size chimera_ctx_switch, .-chimera_ctx_switch

# The first instructions of a new fiber: r12 is its entry, r13 its argument.
	.globl chimera_ctx_trampoline
	.type chimera_ctx_trampoline, @function
chimera_ctx_trampoline:
	movq %r13, %rdi
	callq *%r12
	ud2
	.size chimera_ctx_trampoline, .-chimera_ctx_trampoline
	.section .note.GNU-stack,"",@progbits
