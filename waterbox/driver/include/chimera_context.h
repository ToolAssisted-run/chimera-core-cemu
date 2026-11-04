// A context switch of this core's own, for fibers: what Cemu does with
// ucontext on Linux, which musl does not have. The same code natively and
// in the sandbox.
#pragma once
#include <cstddef>
#include <cstdint>

extern "C" {
// Saves the callee-saved registers, MXCSR and the x87 control word on the
// running stack, stores the stack pointer in *save_sp, and continues on
// load_sp (a stack chimera_ctx_switch saved, or one chimera_ctx_make made).
void chimera_ctx_switch(void** save_sp, void* load_sp);
}

namespace chimera
{
	// A new stack for a fiber: mapped as a stack (a Windows host can deliver
	// a fault only on memory it knows is a stack), with an unmapped page
	// below it. Returns its lowest usable address; size is a multiple of 4096.
	// kind names it in the stack report (StackReport).
	void* StackAlloc(size_t size, const char* kind = "stack");
	void StackFree(void* base, size_t size);

	// The stack report (the hidden setting stack_report): how deep each
	// stack has gone. Stacks start as pages of zeros, so the lowest byte
	// that is not zero is the deepest any frame, or any exception the host
	// delivered on it, ever reached. A stack of another kind is registered
	// for it by hand (a console thread's record, whose padding the
	// recompiler runs on), and unregistered before it is freed.
	void StackRegister(const void* low, size_t size, const char* kind);
	void StackUnregister(const void* low);
	void StackReport(uint64_t frame);

	// The stack pointer chimera_ctx_switch continues on to run
	// entry(param) on the stack [base, base + size). entry must not return.
	void* ContextMake(void* base, size_t size, void (*entry)(void*), void* param);
}
