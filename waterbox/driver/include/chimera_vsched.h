// The machine's one thread and one clock.
//
// Upstream Cemu runs its services (IOSU), its GPU's command processor, its
// timers and its processor emulation on host threads, paced by the host's
// clock. Here each of those "threads" is a task: a fiber with its own stack,
// all of them on ONE host thread, and exactly one runs at a time. A task
// runs until it asks this scheduler for something - to wait, to sleep, to
// give way, to end - and the next is chosen by a rule that reads nothing
// from the host (round robin, in the order the tasks were made). Time is a
// number only this scheduler moves:
//
//   - the console's processor pays for what it runs: every instruction is a
//     cycle of the Espresso's 1.243125 GHz clock (Advance);
//   - a task that polls (SpinYield) pays a little, so that a polling loop
//     still lets time pass;
//   - when no task can run, the clock jumps to the earliest moment one can.
//
// So two runs of the same program do the same things in the same order at
// the same times, natively and in the sandbox alike.
#pragma once
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <mutex>

namespace chimera::vsched
{
	using Cycles = uint64_t;
	constexpr uint64_t kCoreClock = 1243125000; // Espresso core cycles a second
	constexpr Cycles kNever = ~0ull;

	struct Task;

	inline uint64_t CyclesToNs(Cycles c)
	{
		return (uint64_t)(((unsigned __int128)c * 1000000000u) / kCoreClock);
	}
	inline Cycles NsToCycles(uint64_t ns)
	{
		return (Cycles)(((unsigned __int128)ns * kCoreClock + 999999999u) / 1000000000u);
	}

	// a new task, runnable; it first runs at the next switch
	Task* Spawn(const char* name, std::function<void()> body);
	bool Finished(const Task* task);
	void Join(Task* task);
	Task* Current(); // null outside every task
	const char* Name(const Task* task);
	void SetName(const char* name); // of the running task

	// the clock: Espresso core cycles since the console was switched on
	Cycles Now();
	inline uint64_t NowNs() { return CyclesToNs(Now()); }
	void Advance(Cycles cycles);

	// give way to the others; free (the processor's safe points use it)
	void Yield();
	// give way and pay a little: for a task that polls
	void SpinYield();
	// block until ready() is true (asked at every switch) or the clock reaches
	// deadline; returns what ready() said when it woke
	bool Wait(const std::function<bool()>& ready, Cycles deadline = kNever);
	void SleepUntil(Cycles when);
	inline void SleepNs(uint64_t ns) { SleepUntil(Now() + NsToCycles(ns)); }

	// The driver's side. Runs the tasks until the clock reaches `until` (or,
	// when given, until stop() says so at a switch), then returns, every task
	// parked where it was. Called from outside every task.
	void RunUntil(Cycles until, std::function<bool()> stop = nullptr);
	// The point RunUntil runs to (kNever outside it)
	Cycles Target();

	// A thread-local variable of upstream's that every task must have its
	// own copy of (they all share one host thread): its bytes are kept per
	// task and put back when the task runs. A new task starts with zeros.
	void RegisterTaskLocal(void* address, size_t size);

	// counters for the tests
	uint64_t Switches();
	void SetTrace(bool on);
}

namespace chimera
{
	// std::mutex for tasks: a task that finds it taken waits in the scheduler
	class Mutex
	{
	public:
		void lock();
		bool try_lock();
		void unlock();
	private:
		vsched::Task* m_owner = nullptr;
		uint32_t m_count = 0; // for RecursiveMutex
		friend class RecursiveMutex;
	};

	class RecursiveMutex
	{
	public:
		void lock();
		bool try_lock();
		void unlock();
	private:
		Mutex m;
	};

	// std::condition_variable for tasks. A notification wakes every waiter (a
	// spurious wake-up is allowed, and every user loops on its condition).
	class ConditionVariable
	{
	public:
		void notify_one() { m_generation++; }
		void notify_all() { m_generation++; }
		template<typename Lock>
		void wait(Lock& lock)
		{
			const uint64_t g = m_generation;
			lock.unlock();
			vsched::Wait([this, g]() { return m_generation != g; });
			lock.lock();
		}
		template<typename Lock, typename Pred>
		void wait(Lock& lock, Pred pred)
		{
			while (!pred())
				wait(lock);
		}
		template<typename Lock, typename Rep, typename Period>
		std::cv_status wait_for(Lock& lock, const std::chrono::duration<Rep, Period>& d)
		{
			const uint64_t g = m_generation;
			const uint64_t ns = (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(d).count();
			lock.unlock();
			bool woke = vsched::Wait([this, g]() { return m_generation != g; }, vsched::Now() + vsched::NsToCycles(ns));
			lock.lock();
			return woke ? std::cv_status::no_timeout : std::cv_status::timeout;
		}
		template<typename Lock, typename Rep, typename Period, typename Pred>
		bool wait_for(Lock& lock, const std::chrono::duration<Rep, Period>& d, Pred pred)
		{
			const vsched::Cycles deadline = vsched::Now() + vsched::NsToCycles((uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(d).count());
			while (!pred())
			{
				const uint64_t g = m_generation;
				lock.unlock();
				vsched::Wait([this, g]() { return m_generation != g; }, deadline);
				lock.lock();
				if (vsched::Now() >= deadline)
					return pred();
			}
			return true;
		}
	private:
		uint64_t m_generation = 0;
	};

	// std::thread for tasks
	// std::async(std::launch::async, ...) as a task: the result's future is
	// ready when the task has run. Waiting on the future blocks the one host
	// thread, so wait only for a job something else already waited for (as
	// upstream's callers do, on a console event the job signals).
	template<typename F, typename... Args>
	auto Async(F&& f, Args&&... args)
	{
		using R = decltype(std::bind(std::forward<F>(f), std::forward<Args>(args)...)());
		auto job = std::make_shared<std::packaged_task<R()>>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
		std::future<R> result = job->get_future();
		vsched::Spawn("async", [job]() { (*job)(); });
		return result;
	}

	class Thread
	{
	public:
		Thread() = default;
		template<typename F, typename... Args>
		explicit Thread(F&& f, Args&&... args)
		{
			m_task = vsched::Spawn("thread", std::bind(std::forward<F>(f), std::forward<Args>(args)...));
		}
		Thread(Thread&& other) noexcept : m_task(other.m_task) { other.m_task = nullptr; }
		Thread& operator=(Thread&& other) noexcept
		{
			m_task = other.m_task;
			other.m_task = nullptr;
			return *this;
		}
		Thread(const Thread&) = delete;
		bool joinable() const { return m_task != nullptr; }
		void join()
		{
			if (m_task)
				vsched::Join(m_task);
			m_task = nullptr;
		}
		void detach() { m_task = nullptr; }
		void* native_handle() { return m_task; }
	private:
		vsched::Task* m_task = nullptr;
	};

	namespace this_task
	{
		template<typename Rep, typename Period>
		void sleep_for(const std::chrono::duration<Rep, Period>& d)
		{
			vsched::SleepNs((uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(d).count());
		}
		inline void yield() { vsched::SpinYield(); }
	}
}
