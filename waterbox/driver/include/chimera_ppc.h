// The console's processor on the machine's clock (chimera_vsched.h): the
// hooks Cemu's scheduler calls (patch 0004). Defined in
// waterbox/driver/cemu/ppc_clock.cpp.
#pragma once
#include <cstdint>

struct PPCInterpreter_t;

// What the running thread executed since the last call becomes time. Called
// before anything sets hCPU->remainingCycles; chimera_ppc_mark after.
void chimera_ppc_account(PPCInterpreter_t* hCPU);
void chimera_ppc_mark(PPCInterpreter_t* hCPU);
// The clock in Espresso core cycles, what the running thread has executed
// so far included.
uint64_t chimera_ppc_cycles();
// Between two slices of a thread, with no lock held: the other tasks get a
// turn, and the frame may end here.
void chimera_ppc_safepoint();
// No thread can run on any core: wait for one to become runnable or for the
// next of the console's timed events.
void chimera_ppc_idle();
