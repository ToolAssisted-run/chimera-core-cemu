// Cemu's fibers (util/Fiber/Fiber.h) on this core's own context switch
// instead of ucontext. Each fiber's stack is mapped as a stack.
#include "util/Fiber/Fiber.h"
#include "chimera_context.h"

#include <sys/mman.h>

#include <algorithm>
#include <map>
#include <string>
#include <vector>

extern "C" void chimera_ctx_trampoline();

namespace chimera
{
	namespace
	{
		struct StackRecord
		{
			const uint8_t* low; // the lowest usable byte
			size_t size;
			const char* kind;
		};
		std::vector<StackRecord>& Stacks()
		{
			static std::vector<StackRecord> stacks;
			return stacks;
		}
		// the deepest of each kind's stacks that were freed
		std::map<std::string, size_t>& Freed()
		{
			static std::map<std::string, size_t> freed;
			return freed;
		}
		size_t Depth(const StackRecord& s)
		{
			const uint64_t* w = reinterpret_cast<const uint64_t*>(s.low);
			for (size_t i = 0; i < s.size / 8; i++)
				if (w[i] != 0)
					return s.size - i * 8;
			return 0;
		}
	}

	void StackRegister(const void* low, size_t size, const char* kind)
	{
		Stacks().push_back({static_cast<const uint8_t*>(low), size, kind});
	}

	void StackUnregister(const void* low)
	{
		auto& stacks = Stacks();
		for (size_t i = 0; i < stacks.size(); i++)
		{
			if (stacks[i].low != low)
				continue;
			size_t& deepest = Freed()[stacks[i].kind];
			deepest = std::max(deepest, Depth(stacks[i]));
			stacks.erase(stacks.begin() + (long)i);
			return;
		}
	}

	void StackReport(uint64_t frame)
	{
		struct Kind
		{
			size_t size = 0, count = 0;
			std::vector<size_t> depths;
		};
		std::map<std::string, Kind> kinds;
		for (const StackRecord& s : Stacks())
		{
			Kind& k = kinds[s.kind];
			k.size = s.size;
			k.count++;
			k.depths.push_back(Depth(s));
		}
		for (auto& [name, k] : kinds)
		{
			std::sort(k.depths.rbegin(), k.depths.rend());
			std::string deepest;
			for (size_t i = 0; i < k.depths.size() && i < 6; i++)
				deepest += (i ? " " : "") + std::to_string(k.depths[i]);
			fprintf(stderr, "chimera stacks at frame %llu: %s, %zu of %zu bytes, deepest %s; freed ones at most %zu\n",
				(unsigned long long)frame, name.c_str(), k.count, k.size, deepest.c_str(), Freed()[name]);
		}
		fflush(stderr);
	}

	void* StackAlloc(size_t size, const char* kind)
	{
		const size_t guard = 4096;
		void* p = mmap(nullptr, size + guard, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
		if (p == MAP_FAILED)
		{
			fprintf(stderr, "chimera: no memory for a fiber's stack of %zu bytes\n", size);
			abort();
		}
		mprotect(p, guard, PROT_NONE);
		StackRegister((uint8_t*)p + guard, size, kind);
		return (uint8_t*)p + guard;
	}

	void StackFree(void* base, size_t size)
	{
		StackUnregister(base);
		munmap((uint8_t*)base - 4096, size + 4096);
	}

	// Memory that is a stack without being a fiber's: a console thread's
	// record, whose padding the recompiler runs on (coreinit_Thread.cpp)
	void* StackMap(size_t size)
	{
		void* p = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
		if (p == MAP_FAILED)
		{
			fprintf(stderr, "chimera: no memory for a console thread of %zu bytes\n", size);
			abort();
		}
		return p;
	}

	void StackUnmap(void* p, size_t size)
	{
		munmap(p, size);
	}

	void* ContextMake(void* base, size_t size, void (*entry)(void*), void* param)
	{
		uint64_t* top = (uint64_t*)(((uintptr_t)base + size) & ~(uintptr_t)15);
		top[-1] = (uint64_t)&chimera_ctx_trampoline; // ret
		top[-2] = 0;                                 // rbp
		top[-3] = 0;                                 // rbx
		top[-4] = (uint64_t)entry;                   // r12
		top[-5] = (uint64_t)param;                   // r13
		top[-6] = 0;                                 // r14
		top[-7] = 0;                                 // r15
		// MXCSR (all exceptions masked, round to nearest) and the x87
		// control word (64-bit precision, round to nearest, masked)
		top[-8] = 0x1F80ull | (0x037Full << 32);
		return &top[-8];
	}
}

thread_local Fiber* sCurrentFiber{};

static constexpr size_t kFiberStackSize = 2 * 1024 * 1024;

Fiber::Fiber(void (*FiberEntryPoint)(void* userParam), void* userParam, void* privateData) : m_privateData(privateData)
{
	m_stackPtr = chimera::StackAlloc(kFiberStackSize, "fiber");
	m_implData = chimera::ContextMake(m_stackPtr, kFiberStackSize, FiberEntryPoint, userParam);
}

Fiber::Fiber(void* privateData) : m_privateData(privateData)
{
	m_implData = nullptr; // set by the first switch away from this thread
	m_stackPtr = nullptr;
}

Fiber::~Fiber()
{
	if (m_stackPtr)
		chimera::StackFree(m_stackPtr, kFiberStackSize);
}

Fiber* Fiber::PrepareCurrentThread(void* privateData)
{
	sCurrentFiber = new Fiber(privateData);
	return sCurrentFiber;
}

void Fiber::Switch(Fiber& targetFiber)
{
	Fiber* leavingFiber = sCurrentFiber;
	sCurrentFiber = &targetFiber;
	chimera_ctx_switch(&leavingFiber->m_implData, targetFiber.m_implData);
}

void* Fiber::GetFiberPrivateData()
{
	return sCurrentFiber->m_privateData;
}
