// The GPU context the renderer draws on (the GPU bridge's), for Cemu's GPU
// thread (patch 0007). Defined in waterbox/driver/cemu_driver.cpp.
#pragma once
#include <cstdint>

// An identity of the context the bridge draws on now; it changes when a
// state is loaded and in every new process, and 0 means "cannot tell".
uint64_t chimera_gpu_context_id();
// The renderer is about to be made again on a new context: the driver lets
// go of its own objects (the window's stand-in) first, before anything new is
// made, and makes them again when next asked.
void chimera_gpu_context_forget();
