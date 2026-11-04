// The machine's one thread and one clock: see chimera_vsched.h.
//
// Every task is a fiber (chimera_context.h) on the one host thread that
// calls RunUntil. What a waiting task waits for is a function it hands
// over, asked each time the scheduler chooses who runs next: slower than
// wake-up lists, and it cannot get a wake-up wrong.
#include "chimera_vsched.h"
#include "chimera_context.h"

#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace chimera::vsched
{
	namespace
	{
		constexpr size_t kStackSize = 1024 * 1024;
		// what polling costs: a microsecond of the machine's time
		constexpr Cycles kSpinCost = kCoreClock / 1000000;

		struct Local
		{
			void* address;
			size_t size;
			size_t offset; // into Task::locals
		};
	}

	struct Task
	{
		std::string name;
		std::function<void()> body;
		void* stack = nullptr;
		void* sp = nullptr;
		bool started = false;
		bool finished = false;
		std::function<bool()> ready; // what it waits for; empty: runnable
		Cycles deadline = kNever;
		bool wokeReady = false;
		std::vector<uint8_t> locals;
	};

	namespace
	{
		struct State
		{
			std::vector<Task*> tasks; // in order of creation
			Task* current = nullptr;
			size_t cursor = 0; // index of the task that runs, or ran last
			Cycles now = 0;
			Cycles target = kNever;
			std::function<bool()> stop;
			void* driverSp = nullptr;
			bool inRun = false;
			uint64_t switches = 0;
			bool trace = false;
			std::vector<Local> locals;
			size_t localsSize = 0;
			std::vector<Task*> finished; // stacks to free, once nothing runs on them
			bool stalledNoted = false;
		};

		State& S()
		{
			static State* s = new State();
			return *s;
		}

		void SaveLocals(Task* t)
		{
			if (!t)
				return;
			auto& s = S();
			t->locals.resize(s.localsSize);
			for (auto& l : s.locals)
				memcpy(t->locals.data() + l.offset, l.address, l.size);
		}

		void LoadLocals(Task* t)
		{
			auto& s = S();
			if (t && t->locals.size() < s.localsSize)
				t->locals.resize(s.localsSize, 0);
			for (auto& l : s.locals)
			{
				if (t)
					memcpy(l.address, t->locals.data() + l.offset, l.size);
				else
					memset(l.address, 0, l.size);
			}
		}

		void TaskEntry(void* param);

		// Can t run now? A waiting task whose wait is over is made runnable here.
		bool Runnable(Task* t)
		{
			if (t->finished)
				return false;
			if (!t->ready && t->deadline == kNever)
				return true;
			if (t->ready && t->ready())
			{
				t->wokeReady = true;
				t->ready = nullptr;
				t->deadline = kNever;
				return true;
			}
			if (t->deadline != kNever && S().now >= t->deadline)
			{
				t->wokeReady = false;
				t->ready = nullptr;
				t->deadline = kNever;
				return true;
			}
			return false;
		}

		void FreeFinished()
		{
			auto& s = S();
			for (Task* t : s.finished)
			{
				if (t == s.current)
					continue;
				if (t->stack)
					StackFree(t->stack, kStackSize);
				t->stack = nullptr;
			}
			std::erase_if(s.finished, [&](Task* t) { return t->stack == nullptr; });
		}

		void SwitchTo(Task* from, void** fromSp, Task* to)
		{
			auto& s = S();
			s.switches++;
			SaveLocals(from);
			s.current = to;
			LoadLocals(to);
			if (s.trace)
				fprintf(stderr, "vsched %" PRIu64 " %s -> %s\n", s.now, from ? from->name.c_str() : "(driver)", to ? to->name.c_str() : "(driver)");
			if (to && !to->started)
			{
				to->started = true;
				to->stack = StackAlloc(kStackSize, "task");
				to->sp = ContextMake(to->stack, kStackSize, TaskEntry, to);
			}
			chimera_ctx_switch(fromSp, to ? to->sp : s.driverSp);
			// running again (as `from`)
		}

		// The running task (or the driver, from RunUntil) gives up the host
		// thread: the next runnable task after it runs, or the clock moves, or
		// the frame is over and the driver runs.
		void Schedule()
		{
			auto& s = S();
			Task* self = s.current;
			void** selfSp = self ? &self->sp : &s.driverSp;
			while (true)
			{
				if (s.inRun && (s.now >= s.target || (s.stop && s.stop())))
				{
					if (!self)
						return; // the driver itself
					SwitchTo(self, selfSp, nullptr);
					return;
				}
				const size_t n = s.tasks.size();
				for (size_t i = 1; i <= n; i++)
				{
					const size_t idx = (s.cursor + i) % n;
					Task* t = s.tasks[idx];
					if (!Runnable(t))
						continue;
					s.cursor = idx;
					if (t == self)
						return; // keep running
					SwitchTo(self, selfSp, t);
					FreeFinished();
					return;
				}
				// nobody can run: the clock moves to the earliest deadline
				Cycles earliest = kNever;
				for (Task* t : s.tasks)
				{
					if (!t->finished && t->deadline < earliest)
						earliest = t->deadline;
				}
				if (s.inRun && s.target < earliest)
					earliest = s.target;
				if (earliest == kNever)
				{
					fprintf(stderr, "chimera vsched: nobody can run and nobody waits for a time (clock %" PRIu64 "):\n", s.now);
					for (Task* t : s.tasks)
						fprintf(stderr, "  %-24s %s\n", t->name.c_str(), t->finished ? "finished" : (t->ready ? "waits" : "runnable"));
					fflush(stderr);
					abort();
				}
				if (earliest > s.now)
					s.now = earliest;
			}
		}

		void TaskEntry(void* param)
		{
			Task* t = (Task*)param;
			t->body();
			t->body = nullptr;
			t->finished = true;
			auto& s = S();
			s.finished.push_back(t);
			size_t idx = 0;
			while (s.tasks[idx] != t)
				idx++;
			s.tasks.erase(s.tasks.begin() + idx);
			// the search for the next task starts after the cursor: keep it on
			// the task before the one that ended
			if (s.tasks.empty())
				s.cursor = 0;
			else if (s.cursor >= idx)
				s.cursor = (s.cursor + s.tasks.size() - 1) % s.tasks.size();
			Schedule();
			// a finished task is never chosen again
			fprintf(stderr, "chimera vsched: a finished task ran again\n");
			abort();
		}
	}

	Task* Spawn(const char* name, std::function<void()> body)
	{
		Task* t = new Task();
		t->name = name ? name : "task";
		t->body = std::move(body);
		t->locals.resize(S().localsSize, 0);
		S().tasks.push_back(t);
		return t;
	}

	bool Finished(const Task* task)
	{
		return task->finished;
	}

	void Join(Task* task)
	{
		Wait([task]() { return task->finished; });
	}

	Task* Current()
	{
		return S().current;
	}

	const char* Name(const Task* task)
	{
		return task ? task->name.c_str() : "(driver)";
	}

	void SetName(const char* name)
	{
		if (S().current && name)
			S().current->name = name;
	}

	Cycles Now()
	{
		return S().now;
	}

	void Advance(Cycles cycles)
	{
		S().now += cycles;
	}

	void Yield()
	{
		auto& s = S();
		if (!s.current)
			return;
		Schedule();
	}

	void SpinYield()
	{
		auto& s = S();
		if (!s.current)
			return;
		s.now += kSpinCost;
		Schedule();
	}

	bool Wait(const std::function<bool()>& ready, Cycles deadline)
	{
		auto& s = S();
		if (ready && ready())
			return true;
		Task* self = s.current;
		if (!self)
		{
			fprintf(stderr, "chimera vsched: a wait outside every task\n");
			abort();
		}
		self->ready = ready;
		self->deadline = deadline;
		self->wokeReady = false;
		Schedule();
		// Schedule returns to a task only when it was chosen; a waiting task is
		// chosen only through Runnable, which cleared its wait
		return self->wokeReady;
	}

	void SleepUntil(Cycles when)
	{
		if (when <= S().now)
		{
			Yield();
			return;
		}
		Wait(nullptr, when);
	}

	void RunUntil(Cycles until, std::function<bool()> stop)
	{
		auto& s = S();
		if (s.current)
		{
			fprintf(stderr, "chimera vsched: RunUntil from inside a task\n");
			abort();
		}
		s.target = until;
		s.stop = std::move(stop);
		s.inRun = true;
		// run the tasks; the one running when the clock reaches the target
		// comes back here
		Schedule();
		s.inRun = false;
		s.target = kNever;
		s.stop = nullptr;
		FreeFinished();
	}

	Cycles Target()
	{
		return S().target;
	}

	void RegisterTaskLocal(void* address, size_t size)
	{
		auto& s = S();
		s.locals.push_back({address, size, s.localsSize});
		s.localsSize += (size + 15) & ~(size_t)15;
		for (Task* t : s.tasks)
			t->locals.resize(s.localsSize, 0);
	}

	uint64_t Switches()
	{
		return S().switches;
	}

	void SetTrace(bool on)
	{
		S().trace = on;
	}
}

namespace chimera
{
	void Mutex::lock()
	{
		vsched::Task* self = vsched::Current();
		if (m_owner && m_owner != self)
			vsched::Wait([this]() { return m_owner == nullptr; });
		else if (m_owner == self && self)
		{
			fprintf(stderr, "chimera: a task locked a mutex it holds (%s)\n", vsched::Name(self));
			abort();
		}
		m_owner = self ? self : (vsched::Task*)this; // the driver holds it as itself
	}

	bool Mutex::try_lock()
	{
		if (m_owner)
			return false;
		vsched::Task* self = vsched::Current();
		m_owner = self ? self : (vsched::Task*)this;
		return true;
	}

	void Mutex::unlock()
	{
		m_owner = nullptr;
	}

	void RecursiveMutex::lock()
	{
		vsched::Task* self = vsched::Current();
		if (m.m_owner == self && self)
		{
			m.m_count++;
			return;
		}
		m.lock();
		m.m_count = 1;
	}

	bool RecursiveMutex::try_lock()
	{
		vsched::Task* self = vsched::Current();
		if (m.m_owner == self && self)
		{
			m.m_count++;
			return true;
		}
		if (!m.try_lock())
			return false;
		m.m_count = 1;
		return true;
	}

	void RecursiveMutex::unlock()
	{
		if (--m.m_count == 0)
			m.unlock();
	}
}
