# AGENTS.md - Cemu core for Chimera

This repository builds Cemu (https://github.com/cemu-project/Cemu), a Wii U
emulator, as a core for Chimera (https://github.com/ToolAssisted-run/chimera),
a frontend for tool-assisted speedruns. Cemu is a git submodule at a fixed
commit; every change to it is a numbered patch. It is published as
ToolAssisted-run/chimera-core-cemu; CI (`.github/workflows/chimera.yml`)
builds and tests every push and publishes the package.

Read [docs/BUILDING.md](docs/BUILDING.md) before building and
[docs/PLAN.md](docs/PLAN.md) before changing how the core works: it is the
design log, with what was decided, why, and what was measured.

## Layout

- `extern/cemu` - Cemu, a git submodule at a fixed commit.
- `patches/` - the numbered patches applied to `extern/cemu`.
- `waterbox/CMakeLists.txt`, `cemu-sources.cmake` - this core's build of
  Cemu (the source list is written by `gen-sources.py` from upstream's).
- `waterbox/driver/` - the core itself: `cemu_driver.cpp` (starting the
  console, a frame at a time), `cemu/` (what a frontend owes Cemu: the
  scheduler of tasks and the machine's clock `vsched.cpp`, the processor's
  share of it `ppc_clock.cpp`, the GamePad `gamepad.cpp`, the TV's sound
  `audio.cpp`, the window system, fibers, an offline curl),
  `native/` (the native runner and its OpenGL context).
- `waterbox/driver/wbx-entry.cpp` - the core's exports, the same in both
  flavours; `waterbox/gate-harness.h` the loop both runners share;
  `run-wbx.c` the sandbox runner; `gl-host.c` and `glad/` the GPU bridge's
  host half; `guest/` what the guest's C library lacks.
- `waterbox/waterbox.config`, `file_slots.json`, `default_keybinds.json`,
  `languages/` (26 tables), `package-licenses.json` and `licenses/` - what
  the package declares (`build-package.sh` makes it).
- `waterbox/*.sh` - the dependencies, the builds, the patches, the package,
  the tests (`run-gate.sh`, the core's own; `run-frontend.sh`, the package in
  Chimera's engine); `waterbox/tests/` their helpers.
- `tests/homebrew/` - the Wii U test programs, this repository's own.
- `build/` - every output, ignored by git.

## Build and test

```sh
git submodule update --init
git -C extern/cemu submodule update --init --depth 1 dependencies/ZArchive dependencies/imgui dependencies/Vulkan-Headers
waterbox/build-native.sh -m <miniBox> -j 4
waterbox/build-guest.sh -m <miniBox> -j 4
waterbox/fetch-devkitpro.sh && waterbox/build-homebrew.sh && waterbox/build-test-titles.sh
waterbox/run-gate.sh
waterbox/build-package.sh -m <miniBox> -j 4
waterbox/run-frontend.sh --chimera-root <chimera checkout>
```

Both test scripts must end with `0 failed`. Run them before every commit
that changes the core, and say in the commit what they printed. Every text
a person reads is a key of the language tables, in all 26 languages; the
names check (`waterbox/tests/check-names.py`) and Chimera's
`tools/check-core-texts.py` must pass.

## Rules

- **Never edit `patches/` by hand.** Edit the tree under `extern/cemu`, then
  run `waterbox/make-patches.sh`; `waterbox/apply-patches.sh` must then say
  "already applied".
- **The native run is the reference.** The sandboxed core must give the same
  result for the same input.
- **The same input gives the same run, every time.** Nothing may read the
  host's clock, the host's threads, an address the host chose, or a file
  outside the package. Upstream's threads are tasks of one scheduler
  (`chimera_vsched.h`); a wait, a lock or a sleep in Cemu's code goes
  through it (`CEMU_THREAD`, `CEMU_MUTEX`, `CEMU_CONDVAR`, `CEMU_SLEEP_FOR`),
  and time moves only by what the processor ran.
- **No game, system software, font of the console's or key is committed**,
  packaged or put on a CI runner. The tests that need one are given it by a
  variable (`CEMU_FONTS`, `CEMU_CONSOLE_FONTS`, `CEMU_MII`,
  `CEMU_NUS_GAME`, `CEMU_NUS_DLC`, `CEMU_COMMON_KEY`; docs/BUILDING.md) and
  say SKIP without it, as they do in CI.
- **A new test is seen to fail once** before it is trusted.
- **Write in plain language.** `.md` files are ASCII.
- **Commits**: `type(cemu): what changed`, a short body saying what changed,
  why, and how it was tested with the numbers. Stage paths by name. No
  amending, no force push.
