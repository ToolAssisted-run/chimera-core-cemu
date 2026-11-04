# Cemu core - plan and design log

## What this is

A Chimera core for the Wii U, built on Cemu (cemu-project/Cemu, pinned at
`extern/cemu`). Requested in chimera issue 459.

"A fair try" (the owner's words, below) is taken to mean what it says: this
may not work, and where it does not this file will say so.

Where it stands (2026-11-04): M3 under way. The package (cemu.chimeraCore)
runs this repository's homebrew test programs in Chimera's engine, through
projects, and gives the native reference's result: the machine's clock, the
console's memory, the picture of both screens, the sound, with the
interpreter and with the recompiler, with the GamePad's controls as a movie
pressed them, and a program of its own that draws with the console's GPU
library (GX2: its own shaders, QUADS, an occlusion query). Savestates work,
in the same process and in another (the renderer is made again on every
new GPU context). One game has been tried, New Super Mario Bros. U from
the owner: it plays into its first level (below), on software GL. Since
2026-11-04 the package also runs in Chimera's window (it lacked the
renderer setting the window looks for; see below).

## Owner's decisions

- (user-decided, 2026-11-03) Take on the Cemu core now, as a fair try.
  Report plainly if something does not work.
- (user-decided, 2026-11-03) Renderer: OpenGL first, through Chimera's GL
  bridge; Vulkan may follow later.
- (user-decided, 2026-11-03; changed 2026-11-04) Repository: local only, no
  remote, until the owner's word of 2026-11-04, "push and publish
  everything": it is published as ToolAssisted-run/chimera-core-cemu, from a
  history of one commit that holds no Nintendo key (below, "Published").
- (user-decided, 2026-11-03; changed 2026-11-04) The console's four shared
  fonts are firmware the user gives, pinned by SHA-1. The setting Console
  Fonts has three values, in this order: the stand-in (Lato, the default,
  no file needed), Cemu's copies (the files upstream Cemu ships) and the
  console's own (the files of system title 0005001B-10042400). A project
  naming no value gets the stand-in. Nintendo's fonts are never committed
  or packaged; for local tests the files are in ~/core-content/cemu.
- (user-decided, 2026-11-03) Occlusion queries: a setting, Occlusion
  Queries, with the card's count as the default (as upstream, waited for at
  a fixed point) and fake (every object reported fully visible, the same on
  every card). Its description says that with the card's count a movie may
  need the same kind of graphics card to replay, and that fake makes movies
  replay anywhere at the cost of flares through walls.
- (user-decided, 2026-11-03) CPU mode: the default stays the recompiler.
  The Windows recompiler crash is looked into first; if it is in miniBox's
  Windows fault handling of code made at run time, it goes to the owner as
  a miniBox question with the evidence, and miniBox is not changed here.
- (user-decided, 2026-11-03) A Wii U game for local tests comes from the
  owner. Until then the tests are this repository's own programs. A game is
  copied (never linked) to ~/core-content/cemu with a line in
  ~/core-content/SOURCES.txt, never into a repository, and Windows staging
  copies are removed after the checks.

## The pin

`extern/cemu` is Cemu at 3f8dd91 (2026-11-01, "PPCRec: Code cleanup and
refactoring"), the head of upstream's `main` when the work began. Of its
submodules only ZArchive (the reader of .wua files), imgui and
Vulkan-Headers are checked out; vcpkg, cubeb, metal-cpp and xbyak_aarch64
are not needed.

## The eight questions (chimera docs/new-core.md), answered from the source

- **A new core?** Yes. No core here covers the Wii U.
- **Licence.** MPL-2.0 (`extern/cemu/LICENSE.txt`): modification and binary
  redistribution are allowed; the source of the MPL files must stay
  available, which the pinned submodule and `patches/` are. The patches are
  offered under the MPL, as changes to MPL files.
- **One thread on one clock?** Not as it stands, but closer than most. The
  console's own threads (coreinit's) are already fibers, scheduled by Cemu
  with a quantum of guest cycles; in its single-core modes all three
  processor cores' threads run on ONE host thread. Around that, about ten
  host threads: the IOSU services (FSA, MCP, ACT, ACP, NIM, PDM, BOSS, ODM,
  FPD, the CCR NFC reader, the IOS timer), the GPU's command processor
  (LatteThread), the logger, the input poller, the recompiler's worker
  (upstream has a switch to compile synchronously), and a title launch
  thread. Time is the host's: `PPCTimer` reads the processor's time stamp
  counter, calibrated against the host's clock for three seconds at start.
  The console's calendar is the host's (`time(NULL)` in `cemu_initForGame`).
- **The picture.** OpenGL, Vulkan and Metal renderers. OpenGL first (the
  owner's decision). Cemu's GL renderer asks for a compatibility profile
  (its window creates a default context: no vertex array object is bound,
  some draws pass client-side index arrays); the GPU bridge gives a core
  profile, so those uses will have to change. There is no software
  renderer. Upstream reads some of what the GPU drew back into the console's
  memory (texture readback, occlusion queries, stream-out), so for some
  games the machine will depend on the GPU: to be measured.
- **A recompiler?** An interpreter and an x86-64 recompiler (IML ->
  BackendX64). The recompiler picks instructions by the host's processor
  (AVX, MOVBE, BMI2, LZCNT): they will have to be pinned. It uses no faults
  (no fastmem by signals); its function table is a large reservation.
- **Memory.** A 4 GiB reservation at `memory_base`, regions committed with
  `mprotect`. MEM2 is 1 GiB; MEM1 32 MiB.
- **What the user brings.** Games (installed titles as code/content/meta
  folders, .wua archives, .wud/.wux disc images, which need the user's
  keys.txt) and, for games that use them, the console's shared fonts
  (CafeStd, CafeCn, CafeKr, CafeTw: system title 0005001b-10042400). No game
  is on this machine.
- **A gate on a public runner.** No Wii U software may be committed. The
  tests use homebrew programs of this repository's own, built with
  devkitPPC and wut (free tools) by `waterbox/build-homebrew.sh`.
- **Size.** Cemu's `src/Cafe` is 206,000 lines; the core builds 325 of
  upstream's source files.

## The route

- **M0 - a native headless reference.** DONE 2026-11-03 (below).
- **M1 - one thread at a time on one clock**, natively: the same run twice.
  DONE 2026-11-03 for the homebrew program (below).
- **M2 - the guest build**: the same code in the sandbox, the native result.
  DONE 2026-11-03 for the homebrew program (below).
- **M3 - the picture** (the GL bridge), input, savestates, the package, the
  test script, chimera-run through a project, the contract tests.

## M0: the build (2026-11-03)

Upstream builds with vcpkg and CMake and wants wxWidgets, SDL3, curl, cubeb,
X11, GTK, Wayland, libusb and more. None of that is used:

- `waterbox/CMakeLists.txt` is this core's own build. It takes upstream's
  lists of sources (`waterbox/cemu-sources.cmake`, written by
  `waterbox/gen-sources.py` from upstream's CMakeLists.txt files, with each
  left-out file and the reason in a comment), compiles them into one
  library with upstream's precompiled header and the definitions upstream
  uses, and builds ZArchive and the H.264 decoder (ih264d) from upstream's
  `dependencies/`. The same file builds both flavours.
- The libraries Cemu needs and does not carry are built from release
  archives pinned by SHA-256 (`waterbox/build-deps.sh`, into
  `build/deps/<flavour>`): zlib 1.3.1, zstd 1.5.6, fmt 12.1.0, pugixml
  1.14, glm 1.0.1, rapidjson (24b5e7a), Boost 1.86.0 (headers only) and
  OpenSSL 3.0.15 (libcrypto and libssl, no sockets, no threads, no
  assembly). Of curl only the headers are used: the network code links to
  `waterbox/driver/cemu/curl_offline.cpp`, where every transfer fails with
  "could not connect", in both flavours alike.
- What a frontend owes Cemu is in `waterbox/driver/cemu/`: the window system
  (`WindowSystem.h`, which upstream's wx GUI implements), the command line
  (none), the crash handler (none), the host's processor and memory for the
  log (fixed answers), and fibers (`fiber_chimera.cpp`).
- **Fibers.** Upstream's Linux fibers are ucontext, which musl does not
  have. They are this core's own context switch (`context.S`: the
  callee-saved registers, MXCSR and the x87 control word, as glibc's
  swapcontext keeps them), on stacks mapped as stacks (`MAP_STACK`, with an
  unmapped page below): a Windows host can deliver a fault only on memory it
  knows is a stack.
- Patches, so far three small ones: Cemu does not build without its Vulkan
  renderer (two files name Vulkan's constants or include through its
  header); the OpenGL renderer finds its entry points through the driver and
  draws "the window" into a framebuffer object the driver names; and the CPU
  mode is the driver's (upstream picks the recompiler's mode by how many
  cores the HOST has).

GCC 14.2, Ubuntu 24.04. 325 files of Cemu; the whole native build takes
about six minutes on 6 jobs.

## M0: a program, with no window (2026-11-03)

`tests/homebrew/frames` is a Wii U program written for these tests
(devkitPPC 16.2 and wut 1.9.1, `waterbox/build-homebrew.sh`). It draws on
the TV and the GamePad through OSScreen (no GX2), reads the GamePad, runs a
second thread that waits on a condition, sets a periodic alarm, and keeps
what it did in a "mailbox" in its data (a magic string, the frame number, a
running hash, the buttons, the counts).

`build/native/driver/run-native <program.rpx> <folder> --seconds N` starts
it the way Cemu's own frontend does (`CafeSystem::Initialize`,
`PrepareForegroundTitleFromStandaloneRPX`, `LaunchForegroundTitle`) on an
EGL context with no window (llvmpipe, Mesa, a compatibility profile),
waits, and prints the mailbox. Five host seconds:

| CPU mode | frames | presents | worker | alarms |
| --- | --- | --- | --- | --- |
| single-core interpreter | 285 | 276 | 285 | 97 |
| single-core recompiler | 288 | 285 | 288 | 97 |

The picture read back from the framebuffer object shows the program's
text, "frame 283 hash c501406c / buttons 00000000 worker 284 alarm 97", on
its colour.

Not established: anything about determinism. The run is paced by the
host's clock and its threads. The picture showed it at once: its top 108
rows were another frame's colour than the rest - the GPU thread read the
program's screen buffer while the program was clearing it.

## M1: one thread at a time on one clock (2026-11-03)

Patch 0004 and `waterbox/driver/cemu/vsched.cpp`, `ppc_clock.cpp`.

**Every host thread is a task.** Upstream's threads - the IOSU services,
the GPU's command processor, the IOS timer, the H.264 decoder, the title's
launch, the processor emulation itself - are fibers of one scheduler on ONE
host thread (`chimera_vsched.h`), natively and in the sandbox alike. A task
runs until it asks the scheduler for something: to wait, to sleep, to give
way, to end. The next one is the next in the order the tasks were made that
can run. What a waiting task waits for is a function, asked whenever the
scheduler chooses (slower than wake-up lists, and it cannot get a wake-up
wrong). Upstream's `std::thread`, `std::mutex`, `std::condition_variable`
and sleeps are `CEMU_THREAD`, `CEMU_MUTEX`, `CEMU_CONDVAR` and
`CEMU_SLEEP_FOR` (precompiled.h), which are the standard ones without
`CHIMERA` and the scheduler's with it; the IOSU files, the semaphore and
queue helpers, the logger, the GPU thread and the H.264 decoder use them.
Upstream's few thread-local variables (the running PPC instance, the
scheduler lock's count, the current fiber) are kept per task and put back
when a task runs.

Fibers rather than host threads handing over a baton (the Xenia core's
way): the same code runs natively and in the sandbox with no host thread
at all, a state holds every task's stack in plain memory, and the GPU's
OpenGL context is current on the one thread there is.

**Time is what the processor ran.** Cemu gives each console thread a slice
of cycles (`remainingCycles`) and counts it down as it runs: an instruction
a cycle in the interpreter, a block's instructions at once in the
recompiler, 500 for an HLE call. Every place upstream changes a slice's
budget (a new slice, a yield, interrupts off and on, a callback into PPC
code) first turns what was counted down into time (`chimera_ppc_account`),
so the clock is exactly what ran, at the Espresso's 1.243125 GHz: an
instruction is a cycle of the one clock, whichever of the three cores it
ran on. The console's time base, `OSGetTime`, alarms, the IOS timers,
`HighResolutionTimer` and `GetTickCount` all read that clock.

- **Between two slices of a thread**, with no lock held, the processor gives
  the other tasks a turn (`chimera_ppc_safepoint`): the GPU then runs the
  commands written so far, the IOSU answers what was asked. Upstream ran
  them beside it on other threads.
- **When no console thread can run**, the processor waits for one to become
  runnable or for the next timed event: the next alarm, the next AX audio
  frame (every 3 ms of the machine's clock, where upstream follows the host's
  sound device). When nobody can run, the clock jumps to the earliest such
  moment.
- **The vertical blank** is at exact moments, 59.94 Hz (60000/1001),
  counted from power-on, and a frame of the frontend ends 2 us after its
  blank.
- **The GPU waits** - for a command, for memory a WAIT_REG_MEM names, for a
  semaphore, for a flip - in the scheduler, with the next blank as the
  deadline, where upstream spun.
- **The recompiler compiles a function where it is first met**
  (upstream's own switch, `PPCREC_FORCE_SYNCHRONOUS_COMPILATION`), not on
  a thread of its own.
- **The calendar** is the project's (seconds since 1970, UTC; the console's
  time zone is UTC), not the host's.
- Cemu's log is written as it happens (no thread), stamped with the
  machine's time; the input poller thread and the graphic packs are not
  started.

**What two runs found that one could not.**

- Host addresses in the console's memory: the FSA client's IPC buffer pool
  and every IOSU dispatch record kept a host pointer in emulated memory
  (both found by diffing two dumps of the console's memory,
  `waterbox/tests/diff-ram.py`, then a hardware watchpoint). They hold the
  console's address now (patches 0004 and 0005). Two alarms due at the same
  tick were ordered by their host addresses; they are ordered by age.
- Cemu's own overlay (its notices) was drawn into the picture with a fade
  on the host's clock: one 16x16 icon's pixels differed by 1 between runs.
  The picture is the console's alone (patch 0006).

**Measured** (`frames.rpx`, 300 frames = 5.005 s of the machine, each run a
new process, address space randomisation on):

| run | ends with |
| --- | --- |
| interpreter, twice | clock 6221843111, 1615 task switches, the program's frame 311, memory 736ab2e6e74ddc0e, picture 55e39b68cbf12984 - the same lines at every 60th frame |
| interpreter, the host stalled 300 ms at frame 150 | the same lines |
| recompiler, twice | the same lines as each other (memory 40cc2a050a9df601), another run than the interpreter's |

`waterbox/run-gate.sh` holds the three (`clock:repeats`, `clock:stall`,
`clock:recompiler`), 42 seconds. Each was seen to fail: with the test knob
`CHIMERA_TEST_HOST_CLOCK` (the console's time also counts the host's) every
one fails, and `clock:stall`'s own control is that knob; with the FSA pool's
host pointer put back, the memory differs between runs; with the recompiler
compiling on its own thread again, the recompiler's runs differ.

**Not established.** That it holds for a game: no game is here, and the
homebrew draws only through OSScreen, so GX2, shaders, textures drawn by
the GPU, the GPU's readbacks into the console's memory (texture readback,
occlusion queries: upstream finishes them when the HOST's GPU is done) and
H.264 video have not run. Threads still left to the host, none met by the
homebrew: the online services' asynchronous requests (nlibcurl, nn_olv,
nn_idbe use `std::async` and `std::thread`; with no network they fail at
once, but on a host thread), USB devices (nsyshid), a title launching
another title or ending (coreinit_Misc). The IOSU still writes some files
of the host's (play statistics): the console's storage is not in memory
yet.

## M2: in the sandbox (2026-11-03)

`waterbox/build-guest.sh -m <miniBox>` builds the same CMake project with
miniBox's guest toolchain (`guest-toolchain.cmake`, musl and a static
libstdc++, the large code model), links `build/wbx/core.wbx` and runs
miniBox's `check-wbx.sh` on it (clean: the core declares its own thread
locals, which miniBox gives a guest since 2026-10-03, and nothing is kept
below the stack pointer). The libraries are built for the guest by
`build-deps.sh guest`.

What the guest needed:

- **The core's exports** (`waterbox/driver/wbx-entry.cpp`): `Init` reads
  the project's files as Chimera mounts them - `slots` names the program in
  the `game` slot, `settings` holds the settings (`cpu_mode`,
  `clock_start`, `log`) - and `FrameAdvance`, the picture and the tests'
  own exports (the clock, the task switches, the mailbox, a digest and a map
  of the console's memory). Both runners call the same exports: natively
  directly, in the sandbox through miniBox (`gate-harness.h` is the loop
  both share, so they print the same lines).
- **The machine's filesystem** (`waterbox/driver/memfs.cpp`, the Xenia
  core's, from the Vita3K core): Cemu's own folder and the console's storage
  (mlc01) are a tree in memory under `/chimera`, the same in both flavours;
  the program is grafted in read-only from the mounted file. The C library's
  file calls are defined there.
- **The GPU bridge, in both flavours.** Cemu's OpenGL renderer finds its
  entry points through miniBox's bridge (`chimera_gl_lookup`), whose guest
  half is generated into the core (`waterbox/gen-gl.sh`, from miniBox's
  master list and glad's header). The native runner answers it with the
  same host half the sandbox runner has (`gl-host.c`, the Dolphin core's),
  on an EGL context with no window, so that both draw through the same
  calls. The bridge's context is a **core profile**, and Cemu's renderer was
  written for a compatibility one: no vertex array object of its own, and
  two quads drawn with indices from the host's memory. One vertex array is
  bound for good, the quads are drawn without indices (patch 0002). Its
  "window" is a framebuffer object the driver names; a present is copied
  aside, and the picture is read back when the frontend asks for it.
- What musl does not have: `gettid` (miniBox answers system call 186), the
  X11 type names Cemu's GL headers mention (`waterbox/guest/include`, names
  only), a `linux/version.h` that says "no kernel" (Boost.Asio), EGL's
  platform header (glad's). libpng is built for both flavours (Cemu saves
  screenshots with it).
- **zstd** had to be built without its trace hooks (weak symbols, which a
  static guest resolves to address 0: the link failed on them) and without
  its BMI2 bodies chosen at run time (their target attributes reset
  `-mno-red-zone`; check-wbx refused 4803 operands below the stack pointer).
  A first try passed the flags as `CMAKE_C_FLAGS`, which replaced the guest
  toolchain's own flags; they go in `CFLAGS` now.

**Measured** (`frames.rpx`, each a new process):

| run | result |
| --- | --- |
| native and sandbox, interpreter, 300 frames | the same lines at every 60th frame: clock 6221843111, 1615 switches, memory 736ab2e6e74ddc0e, picture e99669a732556786 |
| native and sandbox, recompiler, 300 frames | the same lines (memory 40cc2a050a9df601) |
| sandbox, a state saved and loaded before each of 120 frames | the lines of the straight run, picture included |
| sandbox, a state of frame 60 (80 MB) loaded by another process, 60 more frames | the machine of the straight run (clock, switches, mailbox, memory); the picture is not: wrong colours in a wrong rectangle |

The core is 340 MB with its debug sections; a frame of the homebrew
program takes about 10 ms in the sandbox on llvmpipe.

`waterbox/run-gate.sh` has seven tests now: `sandbox:native` and
`sandbox:recompiler` (with a control: a run whose calendar starts a day
later is another run), `states:rerecord` and `states:session` (with a
control: `HARNESS_BREAK_STALE_STATE` loads the state of the frame before,
and the run is caught). Each new test was seen to fail: a guest-only change
of one cycle per slice made both sandbox tests fail (memory differed, the
clock at the frame's end did not), and the stale state made both state
tests fail.

**Not established.** The picture after a state is loaded in another
process: the GL objects a state names (textures, programs, the window's
stand-in) belong to the old process's context. The renderer has to be made
again on a new context, as the Xenia, Eden and RPCS3 cores do (M3). And
everything M1 did not establish still holds: only the homebrew program has
run.

## M3: the renderer made again on a new GPU context (2026-11-03)

A state names GL objects - textures, shaders, buffers, the window's
stand-in - of the context it was saved in, and Chimera gives a new context
on every load (since 2026-10-19), as does every new process. Patch 0007
and the GPU thread's part of 0004:

- **When.** The GPU thread keeps the identity of the context its objects
  are in (`s_chimeraGpuContext`, in the machine's memory, so a state carries
  the saving process's). It asks for the live one (`chimera_gl_context_id`)
  wherever it can have been parked when a frame ended: after each of its
  waits, all of them between commands and outside a draw pass. When it
  moved, the renderer is made again there, before the GPU's next call.
- **What.** Everything is let go of first (the texture, shader and buffer
  caches, the output shaders, the OpenGL renderer's own index buffer,
  rectangle and copy shaders, its vertex array, the driver's window
  stand-in and imgui's GL objects), then a new `OpenGLRenderer` is made and
  every cache initialised again, as at boot. In a new context the old
  names name nothing, so deleting them first is harmless, and nothing new
  can be given a number an old call could reach. The imgui contexts and
  fonts are handed to the new renderer (they hold no GPU object; making
  them again crashed in imgui's font code).
- **What comes back from where.** The caches are of the console's memory and
  the GPU's registers, which a state holds: textures are reloaded, shaders
  decompiled again, as they are needed.
- The tests' runners give a fresh context too (`gl-host.c`,
  `chimera_gl_host_state_loaded`, as the engine does;
  `CHIMERA_GL_SAME_CONTEXT_ON_LOAD` keeps the old one), and
  `--new-context N` gives one before every Nth frame.

**Measured** (`frames.rpx`): natively, a new context before each of 120
frames (119 renderers made again) gives the straight run's lines, picture
included (9cffc86b9805459d at frame 120); a state of frame 60 loaded by
another process, sandboxed, ends with the straight run's picture. With the
renderer never told (`CHIMERA_TEST_NO_REMAKE`), the picture is another;
before this change the cross-process picture was a wrong rectangle of
wrong colours. `picture:remade` and `states:session` (now with the picture)
are the tests; 8 tests, three and a half minutes.

**Not established.** What only the GPU holds: a render target a game
draws once and reuses is not in the console's memory, so after a load it
is empty until the game draws it again (the Xenia core fetches such
things into the state before it is taken, `StateSaving`; this core does
not yet). The homebrew program draws through OSScreen, which the GPU
reloads from memory at every flip, so it cannot show this.

## M3: the GamePad, the sound and the screens (2026-11-03)

**The GamePad is the frame's input.** Upstream reads a host controller
through its input manager: dead zones and a radial scaling of the sticks, a
host clock for the button repeat, the mouse for the touch screen. None of
that is a console's. Patch 0008 sends `VPADRead` to the driver instead
(`driver/cemu/gamepad.cpp`), which fills the console's answer from the
frame's controls: every button its own bit, the sticks as given (-32768 to
32767 is -1 to 1, up and right positive), the sticks' directions past half
way as upstream gives them, the repeat counted in the console's time (in
upstream's units). Channel 0 is always a GamePad (upstream says the same
when none is configured, so games that need one start); there is no second
GamePad, no Pro Controller and no Wii Remote yet. The microphone is there
(GamePad connected) and silent. The motion sensors answer "held still, face
up", as upstream does with no sensor.

**The touch screen** is two axes, 0 to 65535 each, a fraction of the whole
picture the core hands out (the rule every core with a pointer follows),
and a button, Touch. A touch lands only where the picture shows the
GamePad's screen; it is converted to the panel's own units with upstream's
formula (x 92 to 3975, y 4095 - 254 down to 147).

**The screens.** A setting, `screens`: the TV above the GamePad (1280x1200,
the GamePad's 854x480 centred under the TV; the default), the TV alone
(1280x720), the GamePad alone (854x480). With the GamePad shown, Cemu's
renderer draws its screen as it would into its pad window; the driver
keeps a stand-in for each window and composes the picture when the
frontend asks. What only shows the TV draws nothing into the GamePad's
part, which stays black. Upstream's TAB key and the GamePad's "show the
other screen" button, which swap the screens, are not there.

**The sound** is what the console's sound library hands the TV during a
frame: 48 kHz stereo, in blocks of 576 pairs (four 3 ms frames of the sound
library at once, as upstream feeds a host device). The TV's channels follow
Cemu's setting, stereo by default. The GamePad's speaker is not heard (in
upstream it is a second sound device, off unless configured).

**Lag frames**: a frame in which `VPADRead` was not called is a lag frame
(`InputWasRead`). The frontend's input-poll callback, when it hands one
over, is called at every `VPADRead`.

**Memory for the tools**: two domains, MEM2 (1 GiB at 0x10000000) and MEM1
(32 MiB at 0xF4000000), and the whole 4 GiB address space as a bus whose
live ranges are the mapped ones (unmapped addresses read 0). The console's
words are big-endian.

**Measured** (`pad.rpx`, a script pressing every button once, the sticks and
the touch screen): what the console read is what was pressed, bit for bit;
600 frames hold 480384 pairs of sound (600 x 48000 x 1001 / 60000 = 480480,
one block of 576 the difference); 57 of 600 frames are lag frames (the
program skips its read every tenth pass); the sandbox gives the native
run's lines, and a state saved and loaded before each of 180 frames
changes nothing. `run-gate.sh`: 14 tests passed, 0 failed, 0 skipped, five
and a half minutes. Seen to fail, on purpose, once each: the touch formula
changed (`input:gamepad`, `input:screens`), every other sound block dropped
(`sound:tv`), every frame said to read the GamePad (`lag:counted`), the
native code changed and the sandbox not rebuilt (`input:sandbox`).

**Not established**: a game. What the GamePad's screen shows in a game that
draws it with GX2 (the homebrew programs draw both screens with OSScreen).
The software keyboard (swkbd), which upstream draws with imgui and types
with the host's keyboard: not reachable from the frame's input.

## M3: the package, the console's fonts, the game's files (2026-11-03)

**The package** (`waterbox/build-package.sh`, into `build/package`): the
system WIIU ("Wii U"); one controller, the Wii U GamePad, as player 1
(eighteen buttons, the two sticks, the touch point; letters U D L R A B X Y
l r z Z + - H < > T, axis headers LX LY RX RY TX TY); the picture's room
1280x1200 with the live size from the core; vsync 60000/1001; sound 48 kHz
stereo; lag frames; memory domains MEM2 and MEM1 and the 4 GiB bus. Five
settings: CPU Mode (recompiler, the default, or interpreter), Screens,
Console Language, Console Fonts and the console's clock at start (a date).
Every text is a key of 26 tables (`waterbox/languages`, written by an AI
model from the English, like Chimera's own; no speaker has read them).
Chimera's contract tests of the package (`test-packages`): 22 passed, 0
failed. The names check (`waterbox/tests/check-names.py`, from the rsdkv5u
core) found three tables that bent "Wii U" ("Wii U-spel"): rephrased.

**The console's fonts** (user-decided, 2026-11-03): firmware the user gives,
the four .ttf files, pinned by SHA-1 (the copies Cemu's repository carries,
which are the console's), required only when the setting Console Fonts is
on the console's (its default). On the stand-in, Cemu gives games its own
free font (Lato, built into Cemu's source). They are grafted where Cemu
looks for its own copies (`resources/sharedFonts` in its data folder) and
Cemu loads them into the console's shared area as it does. Measured with
the fonts program (tests/homebrew/fonts, which hashes what OSGetSharedData
gives): the four files byte for byte with the fonts given, Lato four times
on the stand-in, natively, in the sandbox and as a project's firmware;
the hashes are computed from the files outside the console
(`waterbox/tests/font-hash.py`). The copies used are upstream's, in
`~/core-content/cemu`, never in this repository.

**The game's files**: a Wii U archive (.wua, which can hold a game with its
update and add-on content), a homebrew bundle (.wuhb) or a program (.rpx),
started the way Cemu's own window starts a file it is given (a title is
added to the title list and its base title prepared; a program on its own
is prepared as it is). Measured with `frames.rpx` packed by
`waterbox/build-test-titles.sh` as a title of this repository's own (title
id 00050000-F1C3E000): both are mounted as titles and run as the .rpx does
(the same record and picture at frame 60), natively, in the sandbox and as a
project's game. Disc images (.wud, .wux), installed folders and NUS
folders are not offered: no such file is at hand to try, and a disc image
needs its key.

**Upstream's other threads.** A search of every built source for threads
of the host found, besides those of earlier rounds: the file cache's
writer (a static object: it started a host thread when the core was
loaded), the title launcher and the shutdown after a program exits, the
online services' requests (nlibcurl, nn_olv, nn_idbe) and the USB HID
calls (nsyshid). All now run as tasks of the scheduler (`CEMU_THREAD`,
`CEMU_ASYNC`), or not at all: the shader caches on a disk are switched off
(`LatteShaderCache_Load` returns before opening one; the driver's
precompiled binaries are off), because they were the core's own files,
kept in every state and growing, and a driver's binaries are of one card.
None of these paths is exercised by the test programs.

**GL functions of vendors' extensions.** The GPU bridge carries a wrapper
for every GL function whether the host's driver has it or not, and Cemu
picks a path by the pointer: with `glDepthRangedNV` it sets depth ranges
outside 0..1 (NVIDIA's extension), which on a driver without it was a call
that did nothing (Chimera said "glDepthRangedNV was not run"). Cemu's
loader is now given the four functions of vendors' extensions it uses only
when the driver lists the extension, as a driver's own loader answers; the
list is asked of the context the calls land on now. So a game's depth range
follows the card as it does in upstream on Windows: a picture drawn on an
NVIDIA card can differ from one drawn elsewhere (the console's memory does
not).

**Measured**: `run-gate.sh` 16 passed, 0 failed, 0 skipped (6 min 8 s; new:
firmware:fonts, formats:titles). `run-frontend.sh --chimera-root
~/chimera-scratch-bump` (Chimera 08ee9de1) 5 passed, 0 failed, 0 skipped
(1 min 55 s): through projects made and run by chimera-run, 300 frames of
the frames program leave the sandbox runner's MEM2 and picture; a movie
pressing every control leaves the runner's MEM2, picture and 16 lag
frames; a state around every frame, and the state after frame 90 carried
into another process, end with the straight run; the fonts as a project's
firmware; a .wua as a project's game. Package sha1 0b7039fa (the same bytes
when built twice). Seen to fail, on purpose, once each: the fonts' graft
missing one font (firmware:fonts, engine:fonts: Lato four times), titles
never mounted (formats:titles), two buttons swapped and the default screens
changed in the package (engine:program, engine:input), the GamePad's last
state kept outside the savestate (engine:states: the same process was right,
another process was not), the archive's extension taken out of the file
slots (engine:archive).

How chimera-run counts, for whoever writes the next test: `--frames N`
runs N frames; `--save-state F` and `--screenshot F` are after frame F
counted from 0 (F = N - 1 is the last, and a screenshot of it is
`--final-screenshot`); a state loaded with `--state` plays its movie from
the movie's first line.

**Not established** (then; see the later sections for what has been
since). The greenzone kept across a reopened project
(`video.greenzoneSurvivesReload` is not declared: no test had reopened a
project's history in another process). The window (Chimera's Qt
interface): not run. A real graphics card: not tried (software GL, Mesa
llvmpipe, only).

## M3: drawing with GX2 (2026-11-03)

**A program that draws as games draw.** `tests/homebrew/draw` sets up GX2
(command buffer, context state, a colour buffer, the two scan buffers),
makes a fetch shader with GX2 and uses a vertex and a pixel shader of its
own, written as R700 machine code by hand (CALL_FS, then EXPORT_DONE of the
position and of one parameter; the pixel shader exports its input): GX2's
shader compiler is not free software and no free one was at hand. Each
frame it clears, draws a triangle that moves (TRIANGLES), a square drawn as
QUADS from the vertex buffer and a square drawn as QUADS through an index
buffer, asks how many of the triangle's pixels were drawn (an occlusion
query), and copies the buffer to both screens. Cemu decompiles the shaders
to GLSL and draws them through the GPU bridge. (Trap: the program was first
called gx2.rpx; a program is a module named after its file, and the
console's gx2 library then resolved to the program itself.)

**QUADS on a core profile.** Cemu's OpenGL renderer draws GX2's QUADS and
QUAD_STRIP with GL_QUADS and GL_QUAD_STRIP, which a core profile does not
have: both squares were missing (the draws were refused by the driver).
Patch 0007 (the file's first patch) draws them as triangles, two a quad:
an index list in Cemu's index cache keeps the triangles' indices, and a
draw without one gets an index list of its own, made again on a new
context.

**Answers that came when the card was done.** Two things Cemu reads back
from the card and writes into the console's memory were written whenever
the card had finished, as seen at the next look (at a vertical blank,
after some draws): occlusion query results and texture readbacks. Both are
now waited for where they are looked at, so the console's memory gets them
at the same point of every run, whatever the card's speed. The count a
query gives can still differ between kinds of card: hence the setting
Occlusion Queries (owner, above), whose "fake" ends a query with 0x100000
samples at once (the number upstream itself gives Xenoblade Chronicles X).

**Measured**: the picture of frame 60 is what was drawn (the clear colour,
red in the triangle, green and yellow in the squares, on the TV and on the
GamePad); 120 frames twice are one run; a new GPU context before every
frame changes nothing (shaders, buffers and the colour buffer are made
again from the console's memory) and with the renderer never told the
picture is another; the sandbox draws the native run's pictures, with a
state around every frame and with a state carried into another process;
the query answers 48384, the triangle's area (384 by 252 pixels, halved),
and 0x100000 on fake.

`run-gate.sh`: 20 passed, 0 failed, 0 skipped (17 min 7 s in this run;
7 min 27 s for the 19 before the queries test, with the machine otherwise
idle). `run-frontend.sh`: 5 passed, 0 failed, 0 skipped. Chimera's
`test-packages`: 0 failed. Package sha1 47a9eea8. Seen to fail on purpose:
gx2:draw (an index list of quads uploaded as quads: the draw read past its
indices and the process died), gx2:queries (fake answering 0x100001),
gx2:remade (the quads' own index lists not let go of with the context: the
first draw after a remake died). gx2:sandbox was not broken on its own: it
is input:sandbox's comparison, which was.

**Not established**: textures, uniform blocks, depth buffers, render to
texture, the texture readback path itself (the program does not trigger
one), and every format and state a game uses. A real card.

## On Windows, on the GTX 1060 (2026-11-03)

`waterbox/run-windows.sh` makes projects of the three test programs on
Linux and runs them with Chimera's headless runner for Windows
(chimera-run.exe, cross-built from Chimera 08ee9de1 in a local clone,
`~/chimera-cores/cemu-work/chimera`, with miniBox's host library beside
it), staged under the Windows user's temporary folder and removed after.
The card answered as "NVIDIA GeForce GTX 1060 6GB/PCIe/SSE2", OpenGL
"4.6.0 NVIDIA 581.42", with NV_depth_buffer_float (so Cemu's NVIDIA depth
range is used there, and not on llvmpipe).

**With the interpreter**: the frames program (300 frames), the pad
program with a movie pressing every control (180) and the draw program
(120, GX2, an occlusion query on the card's count) leave on Windows
exactly the MEM2 they leave on Linux, and the same query answer. The
pictures are the same for the first two; the draw program's differs in
208 of 1,536,000 pixels, each by one step, all on the edges of the
triangle (the card's filtering, not the machine).

**With the recompiler**, at first: chimera-run.exe died with an access
violation in the first frame, every time (Init passed: a run of 0 frames
ended well). The Windows Event Log named ntdll (offset 0xa4e99) or an
address in the guest's mmap arena; miniBox wrote no diagnostic. The owner
kept the recompiler as the default and asked for the crash to be looked
into first.

**Why** (2026-11-03, found with MSYS2's gdb.exe on Windows, stopping at
every access violation): Cemu's x64 recompiler keeps the CPU state's
address in RSP while it runs the console's code (`REG_RESV_HCPU` is RSP),
and gives each console thread's record (`OSHostThread`, made with `new`)
128 KiB of padding below the state for that reason: upstream's own comment
says the padding is the stack an exception is delivered on. The recompiled
code faults all the time in the sandbox: miniBox keeps memory read-only
until it is written, to know which pages a state must save, and a write
is a fault it serves. On Linux a fault is delivered on a stack of its own
(sigaltstack). Windows pushes the exception's context on the faulting
thread's stack, which here is the padding, in the heap, and as read-only
as any other clean page; the kernel could not write the context, and the
process ended with no handler having run. The debugger saw it: a write
fault in recompiled code (RIP 0x36f4b949743), then
`KiUserExceptionDispatcher` faulting on its own first push at
0x36f018ccff8, in the heap (sbrk) area. miniBox documents the rule (a
guest must map its stacks with MAP_STACK; on Windows only those are always
writable), and ares met it with its coroutine stacks; this is the same.
So it is the core's to fix, not miniBox's.

**Fix** (patch 0004): in a CHIMERA build, `OSHostThread` is allocated with
`mmap(MAP_STACK)` (`chimera::StackMap`, in the driver's
`fiber_chimera.cpp`). Linux runs the same as before (the gate's
recompiler tests unchanged). The padding is in states; on Windows it may
hold the context of the last fault there, which the core never reads, so
a state's bytes can differ between Windows and Linux there while the
console's memory does not.

**Measured** on the GTX 1060 (`waterbox/run-windows.sh`, both CPU modes
now): the frames (300), pad (180) and draw (120) programs leave Linux's
MEM2 with the recompiler as with the interpreter; and with the pad
program, a state saved and loaded around every frame, a state of frame 90
carried from one Windows process into another, and that Windows state
loaded on Linux all end with Linux's straight MEM2 (windows:states, both
modes). 8 passed, 0 failed, 7 min 26 s. Seen to fail: without the fix the
three recompiler runs end with 0xC0000005; windows:states with the state
taken one frame early.

## Save data, out and in (2026-11-03)

The console keeps its saved games in its storage's `usr/save` folder (the
machine's filesystem, so every state carries them). Out: the four exports
of Chimera's save data group give every file under it, named from the
storage's root (`usr/save/00050000/f1c3e100/user/80000001/chimera.sav`, and
the console's own: `usr/save/system/act/...`, `usr/save/system/pdm/...`),
which Emulator > Export Save Data writes as a zip. In: a project's Save
Data slot takes that zip, and every entry under `usr/save/` is written into
the storage when the machine is switched on, before the console starts,
so it is in the machine every state is taken against. An entry elsewhere,
a damaged zip or a file that is no zip is refused, with the reason's key
(the zip reader is the Xenia core's). Texts in the 26 tables.

A game's save folder is its title's: the test is `tests/homebrew/saves`
as a title of its own (`saves.wua`, title id 00050000-F1C3E100), since a
program on its own gets a made-up title id whose save folder the console's
save library does not make (its writes failed with NOT_FOUND).

**Measured**: after 120 frames the export holds chimera.sav with "CHMS"
and 119 (the frame count at its fourth save); a second run with the zip
in its slot reads 119 at start; a non-zip is refused; the sandbox exports
the same files and reads the zip the same way; through Chimera's engine,
`chimera-run --export-savedata` gives the same file and a project made
with the zip in its Save Data slot reads 119 (savedata:round,
engine:savedata).

**Two machines that were another every run**, found by holding the
sandbox's export to the native one's, and fixed in patch 0004:

- The console's account got a new id each run: `Account.cpp` seeded its
  generator from `std::random_device`, which reads the processor's own
  random numbers (RDRAND), and no sandbox stands in front of that. Every
  generator of the console (the account, `helpers.cpp`, the Skylander,
  Dimensions and Infinity portals) is now seeded with one fixed number
  (`CEMU_RANDOM_SEED()`, 0x43454D55), as the console's own would be the
  same each time it is switched on from the same storage.
- The play records (`usr/save/system/pdm`) took the date from the host's
  clock, and a thread of theirs slept on it. They, the BOSS service's
  timeouts, the NFC reader's timeouts and the IPC wait now take the
  machine's clock (`CEMU_STEADY_NOW()`, `CEMU_SYSTEM_NOW()`: the virtual
  time, from the Clock At Start setting), and their sleeps and waits are
  the scheduler's (`CEMU_SLEEP_FOR`, `CEMU_YIELD`), as the sleeps in
  `sysapp.cpp`, `nn_idbe.cpp` and the Dimensions portal are.

The test script holds it (host:random): no object the sandboxed core is
built from refers to `std::random_device`. Its code is still in core.wbx,
because the C++ library keeps it in the object that also holds
`std::mt19937`; nothing calls it.

**A third**: the saves program's memory still differed between two native
runs, in three bytes of Cemu's own area of console memory (0x0E000000,
outside the MEM1 and MEM2 domains). The older IOSU calls
(`iosuIoctl_pushAndWait`, used by the account service) keep the calling
thread in a queue entry on the console's stack, and kept it as a host
pointer, which moves with where the memory is placed (found with a
watchpoint on the address). It holds the console's address now (patch
0005). Measured: two native runs of the saves program and a sandboxed one
give the same lines to the last byte of memory and the same export.

## The state history kept across processes (2026-11-03)

Reopening a project loads the state history (greenzone) an earlier session
kept. `chimera-run --history-out` keeps it, and `--history-in ... --seek F
--stop-at-seek` seeks into it in another process, as reopening does.

**Measured** with the pad program and its movie, default settings (the
recompiler):

- Linux (llvmpipe): a history of 4.3 MB for 180 frames; another process
  seeking to frame 150 has the straight run's MEM2 and picture
  (engine:history). Controls: frame 100 is another picture, and a history
  kept from a run with nothing pressed gives another machine, so the seek
  used the history's states rather than a replay from power-on.
- Windows (GTX 1060), both CPU modes: a history kept by one Windows process
  and sought into by another, and the same history sought into on Linux,
  give Linux's straight MEM2 and picture at frame 150 (windows:history).
- Chimera's own in-process check (`tests/gpu/run-reopen.sh`: open, play,
  save, close, open again in the same process, load, play on) passes for
  the pad and draw programs on llvmpipe: RAM, sound and picture the same
  for 90 frames after the reload.

**Not declared yet**: `video.greenzoneSurvivesReload` (which keeps a
project's greenzone across sessions) is declared only on evidence. The
evidence from a game is below (New Super Mario Bros. U, over its first
level, 2026-11-04); the declaration is the owner's to decide.

**Not a cost of Windows** (corrected 2026-11-03): the histories of 247 to
263 MB that the Windows test kept for the same 180 frames were my test
folder's doing. Chimera's engine loads libzstd from beside the runner and
writes a history uncompressed when it finds none, and the folder had no
libzstd.dll (the Windows bundle has one). With it beside the runner, the
pad program's 180 frames keep 4.79 MB on Windows and 4.57 MB on Linux,
with as many pages in the deltas (142,697 and 155,939, MB_TRACE_DELTA).
miniBox already compares a Windows stack page with what it held, so a
page never written is not counted. `run-windows.sh` now stages libzstd.dll.

## Cemu's per-game profiles (2026-11-03)

Upstream ships a profile for some titles (`bin/gameProfiles/default`, one
.ini a title id), read when the title starts. This Cemu still reads few of
their options, and 18 of the 236 files set one: `accurateShaderMul = false`
for Super Mario 3D World, Captain Toad: Treasure Tracker and Xenoblade
Chronicles X (the GPU's multiplication done the faster way),
`accurateShaderMul = true` for Breath of the Wild (the default anyway), and
`loadSharedLibraries = false` for Minecraft: Wii U Edition and Lost
Reavers (the console's libraries rather than the game's own copies). The
core had none of them: its data folder is the machine's own filesystem.

They are compiled into the core now (`waterbox/CMakeLists.txt` turns the
folder into `game_profiles.inc`; `driver/cemu/profiles.cpp` writes them
into the data folder at boot), names in lower case as Cemu asks for them
(one upstream file is named in upper case and is found on Windows only;
its option is the default). So a title gets the same profile on every
host, fixed by the core's build. The CPU mode a profile names is not used:
the CPU Mode setting decides, as before.

**Measured** (profiles:shipped): `profiled.wua`, the frames program under
the title id of Super Mario 3D World (USA), 00050000-10145C00, is started
with `gameprofile path: gameProfiles/default/0005000010145c00.ini` and
`Strict shader mul: false` in Cemu's log; `frames.wua` with no profile
and `true`; the sandbox the same. Seen to fail with the profiles not
written: `got [0 0 / 1 1]`.

## What the GPU writes back into the console's memory (2026-11-03)

Games draw into colour buffers the CPU then reads. Cemu copies such a
buffer back from the host's GPU into the console's memory (any buffer in
linear layout, and a few small tiled ones it knows: Cars 3's exposure,
Xenoblade Chronicles X's luminance), on its own schedule: it starts a copy
five draws after the buffer was last drawn into, or when the console
waits for the GPU, and upstream took the result when the host's card had
finished. Patch 0004 already took it where it is looked at, whatever the
card's speed. Nothing had run that path.

`tests/homebrew/readback`: each frame, a 64x16 buffer in linear layout is
cleared to a colour that changes with the frame count, a green rectangle
is drawn over its left half (no edge near a pixel's centre, so every card
draws it alike), the TV's picture is drawn, and after GX2DrawDone the CPU
reads the buffer.

**It found a bug of this core's.** The program hung in its first frame: it
goes idle before it has made any alarm, Cemu keeps "the soonest alarm" as 0
until there is one, and the core's idle wait read that 0 as an alarm due at
once - it waited for nothing again and again, and the machine's clock never
moved. A game that waits for the GPU before its first alarm would have hung
the same way. "No alarm" is now "never" (patch 0004,
`OSHostAlarm::soonestAlarm`).

**Measured** (gx2:readback, engine:readback, windows:readback): every frame
the CPU reads that frame's own picture (inside 0x00FF00FF, outside the
frame's clear colour, none a frame late); two runs are one; the sandbox is
the native run, with a state around every frame and with one carried into
another process; through Chimera's engine (its GL host copies a mapped
buffer into memory of the guest's own) the MEM2 is the sandbox runner's;
on Windows (GTX 1060, both CPU modes) the MEM2 is Linux's (llvmpipe).
The test host of this repository (`waterbox/gl-host.c`) now answers the
bridge's question about a buffer's size with 0: it keeps no copies, and
the guest reads the driver's memory, which it can in one process.

What a card draws differently (the edges of a shape) and a game reads
back would make the console's memory differ between kinds of card, as the
occlusion queries' count can. The test draws nothing a card could draw
differently, so it does not show that; it waits for a game.

## The console's three cores (2026-11-03)

Games put their threads on all three of the console's cores. Cemu's
single-core modes, which this core uses (one host thread, the machine's
clock), run the three cores' threads in turn. `tests/homebrew/cores` pins
a thread to each core (affinity CPU0, CPU1, CPU2); they pass a token round
under one mutex and condition, sleep different lengths now and then, and a
periodic alarm of 7 ms counts beside them, while the main thread only
sleeps a frame at a time.

**Measured** (cpu:cores): each thread finds itself on its own core
(OSGetCoreId, 0x210), none elsewhere; 14955 turns in 120 frames; the alarm
fired 286 times in 2 s (7 ms apart: 285.7); two runs are one with each CPU
mode (the interpreter's and the recompiler's lines happen to be the same
for this program); the sandbox is the native run, with a state around
every frame and with one carried into another process. Seen to fail with
the host's clock let into the machine (`CHIMERA_TEST_HOST_CLOCK`): 2671
turns, 670 alarms, and two runs that differ. On Windows (GTX 1060) the
MEM2 is Linux's with both CPU modes (windows:cores).

## Games in the NUS form, and the common key (2026-11-03)

**Owner's decisions (user-decided, 2026-11-03):**
- The core also takes a game as a zip of its NUS folder (the encrypted
  .app files with title.tmd, title.tik and title.cert, as the console
  downloads them), and the add-on content's NUS zip in a second slot
  (Add-on content). .wua, .wuhb and .rpx stay as they were.
- Upstream builds Nintendo's Wii U common key into the program
  (ncrypto.cpp). The core does not carry it: the user gives it as
  firmware, pinned in the project like the fonts. A NUS project without it
  is refused with a plain message. A .wua needs no key.
- With the game the owner gave (New Super Mario Bros. U and its add-on
  content), measure how deep each stack goes on Linux and Windows before
  shrinking any, and bring the greenzone, readback and occlusion questions
  back as questions.

**The zip**: `driver/cemu/nus.cpp` reads only the zip's central directory
(ZIP64 included). An entry stored without compression is grafted into the
machine's filesystem as a range of the zip (`memfs::graft_range`, read
through one descriptor, never loaded whole). A compressed entry of up to
16 MiB is unpacked into the machine. A larger compressed one is refused
(encrypted files do not get smaller, and unpacking a gigabyte into the
machine would put it in every state). The title's files may sit at the
zip's root or in one folder of it. Names are made lower case, as Cemu asks
for them. The game is mounted at /chimera/game/title and the add-on
content at /chimera/game/aoc, and Cemu opens them as NUS titles
(TitleInfo, FSTVolume::OpenFromContentFolder). Each is opened once first,
so that a key that does not open it is said as such
(error.nus_cannot_decrypt) rather than as a damaged title.

**The key**: firmware `wiiu-common-key.bin`, a file of the key's 16 bytes,
declared with its size and SHA-1 (the hash cannot be turned back into the
key). It is required when the game slot holds a zip or the add-on slot
holds anything (`requiredWhen`). The core reads it at Init and refuses a
file that is not 16 bytes (error.key_not_a_key). Taking the key out of
Cemu is not a patch: a patch that removes a line carries it.
`waterbox/remove-keys.py` rewrites the arrays instead (each becomes zeros;
the common key's is filled by `chimera_common_key()`), before the patches,
on every pass of `apply-patches.sh`, which fails if it cannot say the keys
are gone. `make-patches.sh` writes the files that held a key against a
copy with the keys out, so no patch carries one as a change or as context.
For local tests the key file is made from upstream's source, in
~/core-content/cemu, on the same footing as the fonts.

**Measured**:
- nus:refused (no game needed): a zip with no title.tmd, a file that is no
  zip, a NUS zip and no key, a key that is not 16 bytes, a large
  compressed entry, and add-on content beside a .wua are each refused with
  their reason's key, natively and in the sandbox. Seen to fail with the
  key check taken out (got [1 1 0 1 1 1]).
- key:absent: the key, as its bytes and as text in both cases, is in none
  of 57 files (core.wbx, the native runner, every file of the package). The
  test reads the key from the local key file; nothing of the key is in the
  repository. Seen to fail with the key file itself added to the files
  looked in.
- nus:game: New Super Mario Bros. U read from its zip: 300 frames twice the
  same, three different pictures, the sandbox the same; a key of 16 zero
  bytes refused with error.nus_cannot_decrypt. Seen to fail with that wrong
  key in the first run.
- The add-on content mounts ("DLC: /chimera/game/aoc/title.tmd [NUS]" in
  Cemu's log). Its effect has not been seen: the game does not get past its
  title screen yet (below).
- Through Chimera's engine: a project of the game with its add-on content
  and the key leaves the sandbox runner's MEM2 after 1500 frames
  (recompiler). The same project with no key given, or with a key of zeros,
  is refused with the core's message.

Upstream also builds in other Nintendo keys: SpotPass, amiibo, the NFC
reader's and the icon database's (18 arrays in 7 files, the common key
among them). Owner's decisions (user-decided, 2026-11-03): all of them go
out the same way, and the functions that need one fail as they would
without it. `waterbox/tests/find-keys.py` (the test keys:absent) reads
each key from upstream's own source and looks for it, as bytes and as
text, in the core, the runner, the package and the repository.

**Before this repository is ever published** (user-decided, 2026-11-04):
the local history holds the NFC reader's key and its IV, as context lines
of patch 0004, from 178aae2 (which added the patch) to the commit before
673720e (which took them out); `find-keys.py --history` names those two
commits, the changes that added and removed the lines. The history is
rewritten to take them out before anything is pushed anywhere; it was not
rewritten now.

## New Super Mario Bros. U (2026-11-03)

The owner's copy (NUS, title 00050000-10101D00, with its add-on content
0005000C-10101D00), in ~/core-content/cemu only.

**What was seen** (llvmpipe, native and sandbox, recompiler and
interpreter): the game boots and shows "Save data has been created", the
two SpotPass dialogs and "SpotPass has been activated" (each dismissed with
A), then its title screen: the island, Mario, Luigi and the two Toads, the
logo, "Press A!" on the TV and "Tap the screen!" on the GamePad, drawn as
they should be. 1800 frames take 90 s natively on llvmpipe. Two runs are
one, and the sandbox is the native run to the last byte of the console's
memory and the sound.

**Where it stops**: about 30 frames after A is pressed on the title screen,
the game reads through a null pointer in its own code (r31 = 0, lwz
0x18(r31)) and the process ends. It is the same with the interpreter, and
with the console's own fonts. At boot the game asks for the console's Mii
resource, /vol/storage_mlc01/sys/title/0005001b/10056000/content/
FFLResMiddle.dat (a system title every console has, which neither this core
nor upstream Cemu provides), and the open fails. The crash's call stack runs
through the game's code next to its Mii library code (found with a
breakpoint on FSOpenFile for that file).

Owner's decision (user-decided, 2026-11-03): a setting, Mii Resource: the
console's (the default; the files as firmware, pinned by SHA-1, needed only
with that value) or none. The core grafts the files where a console keeps
them (mlc01/sys/title/0005001b/10056000/content) and the runners take them
with `--mii <folder>`.

The title itself (from the owner's Wii U system files, decrypted with the
core's NUS reader, `run-native --extract-nus <zip> <key> <folder>`) holds
four files: FFLResHigh.dat (3,871,104 bytes), FFLResHighLG.dat
(3,856,000), FFLResMiddle.dat (1,490,368) and FFLResMiddleLG.dat
(1,489,600). All four are declared as firmware with those sizes and their
SHA-1, required when Mii Resource is the console's. An FFLResHigh.dat from
elsewhere (4,579,008 bytes) is another file and is not pinned.

**Measured** (firmware:mii, engine:mii): the mii test program
(tests/homebrew/mii, in mii.wua, since a program on its own is not given
the console's storage) reads the four files where a console keeps them
and finds them byte for byte; with none it finds none; with the console's
and a file missing, the project is refused with the file's name; the
sandbox finds the same. Seen to fail (2026-11-04) with the core grafting
each file from the next: the program found the files in the wrong order,
natively, in the sandbox and through a project.

**Tried**: with the title's four files given, the game opens them and
stops at the same instruction with the same call stack. So the Mii
resource is not the whole cause.

**Why it stopped, and the fix (2026-11-04).** Followed back from the null
pointer with watchpoints (gdb on the native runner, with the interpreter)
and the game's code read out of the console's memory (devkitPPC's
objdump, with the names of the functions it imports from Cemu's
libraries):
- The request that stops is pushed by the game's code for its list of
  the players' Miis (one for each user account on the console, from
  nn::act GetNumOfAccounts and GetSlotNo). With that list empty, the code
  still pushes the list's first entry, whose Mii object is null, and the
  code that takes the request reads through it.
- The list stays empty because the game's Mii library (FFL, built into
  the game) never becomes ready. After the resource it opens its database
  of Miis, /vol/storage_mlc01/usr/save/00050010/1004a100/user/common/db/
  FFL_HDB.dat, in the Mii Maker's save folder; the file is not there, and
  making it fails because the folder is not there either (Cemu's log:
  "File create failed").
- Upstream's window makes that folder, the Mii Maker's folders of the
  other two regions, the folders titles are installed in and the system's
  lists of languages and countries in every new console storage
  (CemuApp::CreateDefaultMLCFiles). This core has no window, and nothing
  made them. The driver now makes the same folders and files at boot, in
  the machine's filesystem (`MakeStorageDefaults` in cemu_driver.cpp).
- The test storage:defaults holds it with a program of this repository's
  own (tests/homebrew/storage, in storage.wua): a file can be made in each
  of the three Mii Maker database folders, the title folders are there,
  and the two lists are upstream's to the byte (computed from upstream's
  own country table by `waterbox/tests/storage-lists.py`), natively and in
  the sandbox. Seen to fail with the call taken out: the program found one
  folder of six and no lists.

**Measured** (llvmpipe; with the Mii resource given, the stand-in fonts,
no add-on content): the game gets past its title screen.
Played: A on the title screen, the file select ("Story Mode", three new
files, "Play with Mii"), A on file 1, the controller question (Wii U
GamePad), "Begin Story Mode?", the opening scene to its end (the castle,
Bowser's airships, the brothers thrown to Acorn Plains), the camera's
tour of the world map, Acorn Plains on the map, right to the first level
and A: Acorn Plains Way, where Mario stood until a Goomba took a life,
went back to the map, entered again and ran right with jumps, to frame
11,900. Nothing further was played. With the interpreter natively to
6,500 and in the sandbox to 11,900 (from states taken on the way, at
6,500, 8,300 and 10,100); with the recompiler natively to 12,000, which
reaches the level with the same presses (its clock is not the
interpreter's, so the two runs are not the same frame for frame). Two
sandboxed runs to frame 2,400 are one, the console's memory included.
With Mii Resource on none, the game still stops after its title screen,
at the same instruction: the library needs the resource as well as the
folder, and the setting's text says a game that draws Miis may fail.

**Two native runs are no longer one past the title screen**, in the
console's memory alone: the picture, the sound, the clock and the task
switches agree. Cemu runs the game's zlib calls (zlib125.rpl) on the
host's zlib, whose working state (7,160 bytes, taken from the game's own
allocator) is in the console's memory and holds host addresses (of the
stream, of its window). Natively those move from run to run with where
the system places memory: at frame 2,100, 16 words in two pages differ
(found with a watchpoint on one of them: inflateInit2_, from
zlib125Export_inflateInit2_). In the sandbox every process places memory
the same way, so the sandbox repeats; at frame 2,100 it differs from a
native run in those two pages only. A state carries them. States loaded
in another sandboxed process, on Linux and on Windows, give the same
machine over a level (below); whether one of them was taken while a
stream was in the middle of its work is not known.

**Readback and occlusion queries**: to the title screen (1850 frames), the
game asked for no occlusion query and no texture readback (breakpoints on
LatteQuery_BeginOcclusionQuery, LatteTextureReadback_Initate and _Start-
Transfer, none hit). In play (2026-11-04, native, the recompiler, counted
every 100 frames): none either, in 11,900 frames that take in the file
select, the opening scene, the world map and two goes at Acorn Plains Way
(some 800 frames in the level, judged from the pictures). The same
breakpoints count 121 queries in 120 frames of the draw program and 121
readbacks in 120 frames of the readback program, so they do see them. So
far nothing this game does depends on the card's answers; other levels
are not tried.

**The history across processes** (Linux, llvmpipe, through the engine): a
history of 499 MB kept over 1800 frames by one process, sought into by
another at frame 1700, gives the straight run's MEM2 and picture at
1700; frame 1600 is another
picture; a history kept from a run with nothing pressed gives another
machine.

**The history across processes, over a level** (2026-11-04; through the
engine, the recompiler, the Mii resource as firmware, no add-on content):
a project of the 11,900-frame movie of the play above. One process kept
its history (1.99 GB, 20 minutes on llvmpipe). Another loaded it and
sought to frame 11,750 straight away, before playing anything (`--play 0
--seek 11750 --stop-at-seek`, as a reopened project seeks): 29 s, where
playing to that frame takes 14 minutes, so the seek came from the
history's states, and the machine is the straight run's, MEM2 and
picture, in the level. A seek after playing the movie through first
(chimera-run's usual way) gives the same; frame 11,600 is another
picture. The same Linux history, sought into the same way on Windows
(GTX 1060, chimera-run.exe of the same Chimera, the game, the history and
the firmware staged in the temporary folder for the check and removed
straight after): Linux's straight MEM2 at 11,750, in 24 s; the picture
is the card's (another hash, the same scene).

**Declared** (user-decided, 2026-11-04): the package declares
`video.greenzoneSurvivesReload`, with this evidence written beside it in
waterbox.config. The two checks the owner asked for after it, with the
package of the declaration:
- **A history kept on Windows over the level** (GTX 1060, chimera-run.exe
  of the same Chimera; the game, the history and the firmware staged in the
  temporary folder and removed straight after): one Windows process played
  the 11,900 frames in 218 s and kept a history of 1.99 GB. Another Windows
  process sought into it at frame 11,750 with nothing played first: 16 s,
  Linux's straight MEM2; the picture is the card's, and frame 11,600 is
  another. A Linux process sought into the same Windows history: 39 s,
  Linux's straight MEM2 and picture.
- **Chimera's window reopening the project** (Linux, llvmpipe, the window
  of the same Chimera run headless, its settings, cache and recovery files
  in a folder of the test's own; a Lua script of the window drives it,
  kept outside this repository): one window opened the project, played the
  movie to 11,900 through TAStudio's seek (21 minutes), saved it and quit,
  which writes the greenzone into the cache (2.64 GB). A second window
  opened the project and sought to frame 11,750: 67 s, and MEM2's SHA-256
  there is the straight run's. What the window's `tastudio.hasstate`
  answered (false both times, in the window that kept the history too) is
  not read as evidence either way. Not tried: the window on Windows, and
  the window reopening a project inside one process (a script stops once
  it opens a project).

**On Windows** (GTX 1060, chimera-run.exe, the game staged in the
temporary folder for the checks and removed after): through the engine,
the MEM2 after 1800 frames is Linux's. A history kept by one Windows
process (1.56 GB for 1800 frames, uncompressed, because the test folder
had no libzstd.dll; see "Not a cost of Windows" above), sought into by
another at frame 1700, gives that card's straight run: the same MEM2
(which is also Linux's) and the same picture. The picture at frame 1600 is
another. The pictures differ from llvmpipe's, as a card's do.

## Console Fonts: three values (2026-11-04)

**Owner's decision (user-decided, 2026-11-04):** Console Fonts has three
values, in this order: the stand-in (Lato), the default; Cemu's copies
(the files upstream Cemu ships, pinned as before); the console's own (the
files of system title 0005001B-10042400, pinned from the copies decrypted
from the owner's system files). A project naming no value gets the
stand-in, with no file. Each value's text says where its files come from,
in the 26 tables.

**What changed**: the setting's options are `standin`, `cemu` and
`console`. Each font file is declared twice under its one id (CafeCn.ttf,
CafeKr.ttf, CafeStd.ttf, CafeTw.ttf): Cemu's copy, required when the value
is Cemu's copies, and the console's (13,110,604, 2,260,660, 2,442,464 and
8,229,724 bytes), required when it is the console's own. The core treats
the two alike: it mounts the four files where Cemu looks for its own
copies, and the package decides which files are wanted.

**Measured**: firmware:fonts gives the console both sets byte for byte
(natively, and Cemu's copies in the sandbox) and the stand-in without
files; engine:fonts does the same through projects, a project naming no
value gets the stand-in, and one asking for files it is not given is
refused. Seen to fail with the core grafting each font from the next file:
both sets came out wrong in both tests.

**A project made before the change** that names `console` pinned Cemu's
copies under that value, which meant Cemu's copies then. The engine holds a
project to the hashes it pinned, so such a project opened, as the console's
own, and the console got Cemu's copies: the same machine as before, with a
value that no longer said what its files were (measured with engine:fonts).

Owner's decision (user-decided, 2026-11-04): the core refuses the console's
own when the files given are Cemu's copies, with a sentence telling the
user to choose Cemu's copies. A plain refusal, no conversion. The core
knows Cemu's copies by their sizes (all four differ from the console's,
and both are declared in waterbox.config) and says
error.font_is_cemu_copy with the file's name, in the 26 tables: "CafeCn.ttf
is Cemu's copy of a console font, not the console's own. Set Console Fonts
to Cemu's copies." Measured: engine:fonts makes a project on Cemu's copies,
rewrites its value to `console` as an older project has it, and the engine
refuses it with that sentence; firmware:fonts gives the native runner
Cemu's copies with the console's own value and it refuses them. Seen to
fail with the check switched off: the older project opened.

## The renderer setting, which Chimera's window looks for (2026-11-04)

Opening the game's project in Chimera's window (headless, Linux) stopped
at once: "Cemu draws with OpenGL on the graphics card, and the core was
given no graphics card." The window gives a core the GPU bridge only when
the project's renderer setting has a value ending in `-hw` (Chimera's
docs/gpu-bridge.md, "Turning it on"), and this package had no renderer
setting at all. Every test before used chimera-run, whose `--gpu` gives
the bridge without asking, so none of them saw it: until now the core
could not have run in the window.

The package now declares a renderer setting with the one value there is,
`opengl-hw` ("Hardware (OpenGL)"), the default, with its texts in the 26
tables. The core refuses any other value (setting.renderer.unknown). A
project that names no renderer, as every one made before does, gets the
default. Measured: the frames program's project, and the game's, open in
the window with llvmpipe's OpenGL ("chimera gl: 4.5 (Core Profile) Mesa
25.2.8 ... llvmpipe"). The new test window:program (run-frontend.sh) opens
the frames program's project in the window, run headless, and a Lua script
of the window (`waterbox/tests/window-seek.lua`) seeks to frame 300 and
takes MEM2's SHA-256: it is chimera-run's. Seen to fail with a copy of the
package without the renderer setting: the window refused the project
with the sentence above.

## How deep the stacks go (2026-11-03)

The owner asked for the stacks' real depths with the game running before
any is made smaller. The hidden setting `stack_report` prints, every 600
frames, the deepest point each kind of stack has reached. Stacks start as
pages of zeros, so the lowest byte that is not zero is the deepest any
frame, or any exception Windows delivered on it, ever got. Kinds:
- task: the scheduler's tasks, 1 MiB each;
- fiber: Cemu's fibers, 2 MiB each, one for each console thread among them;
- record: the 128 KiB of padding in each console thread's record, which
  the recompiler runs on (see the Windows section).

New Super Mario Bros. U with its add-on content, through the engine, 1800
frames (boot, the four dialogs, the title screen; nothing further can be
played yet):

| kind | stacks | size each | deepest, Linux | deepest, Windows |
| --- | --- | --- | --- | --- |
| fiber | 27 | 2 MiB | 36,296 bytes | 36,296 bytes |
| task | 17 | 1 MiB | 35,480 bytes | 35,480 bytes |
| record | 26 | 128 KiB | 40 bytes | 3,272 bytes |

On Windows an exception is delivered on the stack in use, and the record
padding shows it: 3,272 bytes there, against 40 on Linux, where a fault is
delivered on a stack of its own. The fibers and tasks went no deeper on
Windows. Together the stacks are about 74 MiB. miniBox counts a page of
them only when it differs from what it held when it became a stack, so
pages never written cost a state nothing on either system. The larger
Windows history that started this was the missing libzstd.dll (above),
not the stacks.

Into the first level (2026-11-04; native, Linux, the recompiler, the Mii
resource given, 12,000 frames, the presses of the play above): fibers
36,360 bytes at most, tasks 60,696, records 40. Both deepest points were
reached by frame 1,800, before the title screen was left; the file
select, the opening scene, the map and the level went no deeper. Windows
was not measured in play.

Owner's decision (user-decided, 2026-11-04): miniBox is not changed and
no stack is made smaller. The earlier decision to change miniBox for the
Windows stacks was made on the wrong measurement (the missing
libzstd.dll) and is withdrawn. The libzstd.dll staging in run-windows.sh
and the corrected text above stay.

## Published (2026-11-04)

Owner's decision (user-decided, 2026-11-04): "push and publish everything".
The coordinator makes the repository, ToolAssisted-run/chimera-core-cemu,
pushes it, and handles Chimera's roster, the website and issue 459. This
repository's part:

- **The history.** The local history held the NFC reader's key and its IV
  as context lines of patch 0004, from 178aae2 to the commit before
  673720e (owner's decision of 2026-11-04: rewritten before publishing).
  The published history starts with one commit that holds the final tree
  and folds the local history into it; `find-keys.py --history` finds no
  key in any file or change of it. The local branch with the old history
  is kept here and not pushed.
- **CI** (`.github/workflows/chimera.yml`, as the other cores' are):
  Chimera's main branch is checked out and built (the engine, chimera-run,
  the window), then miniBox, the libraries, the native reference, the
  sandboxed core and the test programs (devkitPPC and wut from their
  pinned container layer); `run-gate.sh` and `run-frontend.sh` run, every
  test a runner can run must pass, the package is built and stamped with
  the commit, Chimera's contract tests run on it, and Chimera's reusable
  workflow publishes it (`dev` on a push to main, a nightly at 04:00 UTC
  when main moved). A runner has no game, no console file and no key:
  firmware:fonts, firmware:mii, nus:game, engine:fonts, engine:mii and
  engine:nus say SKIP there, and `run-windows.sh` is not run. They run
  where the files are, named by variables (docs/BUILDING.md), which no
  longer default to a folder of this machine's.
- **What changed for a runner**: the memory cap (`systemd-run --user`) is
  used only where the user's systemd can make one; the fonts tests are
  split so that the stand-in and the refusals run with no file
  (fonts:standin, engine:standin); the text checks are tests of the gate
  (texts:names, and texts:chimera, Chimera's `tools/check-core-texts.py`
  on a copy of this repository's own files, since CI checks Chimera out
  inside it).

**The workflow replayed here** (2026-11-04), on a fresh clone of the
published commit's tree (this paragraph aside), with Chimera main
f0cc3a1e built as `tools/ci-chimera.sh` builds it, and its steps run one
by one as a runner would, with no file of the console's, no game, no key
and no user systemd. The installs, Chimera's build and the downloads
(devkitPPC's layer, the libraries' tarballs) were done by hand once
first. Every step passed: run-gate.sh 30 passed, 0 failed, 3 skipped
(firmware:fonts, firmware:mii, nus:game); run-frontend.sh 9 passed, 0
failed, 3 skipped (engine:nus, engine:fonts, engine:mii); Chimera's
contract tests, 0 failed. The steps took 38 minutes on this machine
(20 cores, four jobs at a time): the native reference 6, the sandboxed
core 8, the test programs a few seconds, the gate's tests 20, the package
half a minute, the frontend tests 3. A runner's four slower cores, with
Chimera, Qt, miniBox and the libraries to build or fetch on a cold cache,
should take about one and a half to two hours; the job's limit is five.
