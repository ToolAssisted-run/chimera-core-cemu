// The console's processor on the machine's clock (chimera_ppc.h).
//
// Cemu gives each thread a slice of cycles (hCPU->remainingCycles) and
// counts it down as it runs: an instruction a cycle in the interpreter, a
// block's instructions at once in the recompiler. Every change Cemu makes to
// a slice's budget is wrapped in chimera_ppc_account (what was counted down
// since the last mark becomes time) and chimera_ppc_mark (a new mark), so
// that exactly what ran moves the clock.
#include "chimera_ppc.h"
#include "chimera_vsched.h"

#include "Cafe/HW/Espresso/PPCState.h"
#include "Cafe/OS/libs/coreinit/coreinit_Alarm.h"
#include "Cafe/OS/libs/coreinit/coreinit_Time.h"
#include "Cafe/OS/libs/snd_core/ax.h"
#include "util/helpers/Semaphore.h"

namespace coreinit
{
	extern CounterSemaphore g_coreRunQueueThreadCount[3];
}

extern uint64 ppcCyclesSince2000TimerClock;

namespace
{
	PPCInterpreter_t* s_markOwner = nullptr;
	sint32 s_mark = 0;
}

void chimera_ppc_account(PPCInterpreter_t* hCPU)
{
	if (hCPU != s_markOwner)
	{
		s_markOwner = hCPU;
		s_mark = hCPU->remainingCycles;
		return;
	}
	const sint64 used = (sint64)s_mark - (sint64)hCPU->remainingCycles;
	if (used > 0)
		chimera::vsched::Advance((uint64_t)used);
	s_mark = hCPU->remainingCycles;
}

void chimera_ppc_mark(PPCInterpreter_t* hCPU)
{
	s_markOwner = hCPU;
	s_mark = hCPU->remainingCycles;
}

#if !defined(CHIMERA_GUEST)
#include <chrono>
// For the tests only (the negative control of clock:stall): with
// CHIMERA_TEST_HOST_CLOCK set, the console's time also counts the host's
// time since the first reading, as upstream's does, so that a run which
// stalls the host becomes another run.
static uint64_t TestHostClockCycles()
{
	static const bool on = getenv("CHIMERA_TEST_HOST_CLOCK") != nullptr;
	if (!on)
		return 0;
	static const auto start = std::chrono::steady_clock::now();
	const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();
	return chimera::vsched::NsToCycles((uint64_t)ns);
}
#endif

uint64_t chimera_ppc_cycles()
{
	if (PPCInterpreter_t* hCPU = PPCInterpreter_getCurrentInstance())
		chimera_ppc_account(hCPU);
#if !defined(CHIMERA_GUEST)
	return chimera::vsched::Now() + TestHostClockCycles();
#else
	return chimera::vsched::Now();
#endif
}

void chimera_ppc_safepoint()
{
	if (PPCInterpreter_t* hCPU = PPCInterpreter_getCurrentInstance())
		chimera_ppc_account(hCPU);
	chimera::vsched::Yield();
}

void chimera_ppc_idle()
{
	using namespace chimera::vsched;
	// the console's timed events: the next alarm (in OSGetTime's ticks, a
	// twentieth of the core clock, counted from 2000) and the next AX frame
	Cycles deadline = kNever;
	const uint64 alarmTick = coreinit::alarm_getSoonestTick();
	if (alarmTick != std::numeric_limits<uint64>::max())
	{
		const uint64 sinceBoot = alarmTick > ppcCyclesSince2000TimerClock ? alarmTick - ppcCyclesSince2000TimerClock : 0;
		deadline = std::min<Cycles>(deadline, sinceBoot * 20);
	}
	const uint64 ax = snd_core::AXOut_nextUpdate();
	if (ax)
		deadline = std::min<Cycles>(deadline, ax);
	Wait([]() {
		return !coreinit::g_coreRunQueueThreadCount[0].isZero() || !coreinit::g_coreRunQueueThreadCount[1].isZero() || !coreinit::g_coreRunQueueThreadCount[2].isZero();
	}, deadline);
}
