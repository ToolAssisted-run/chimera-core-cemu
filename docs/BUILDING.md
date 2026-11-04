# Building the Cemu core

This builds the native reference, the sandboxed core, the package and the
test programs. It has been done on Ubuntu 24.04 with GCC 14.2.

## What you need

- GCC 14 (`gcc`, `g++`, first on PATH: miniBox builds the guest's C++
  library for the gcc it finds), CMake 3.21 or later, Ninja, Python 3,
  curl.
- EGL and an OpenGL driver with a 4.5 core profile, for the GPU bridge both
  runners draw through (`libegl-dev`; Mesa's llvmpipe will do, it is what
  the tests use).
- A Chimera checkout with miniBox's host library and C++ guest toolchain
  built (its `extern/chimera-common-minibox`, named `<miniBox>` below):

```sh
mb=<chimera>/extern/chimera-common-minibox
meson setup "$mb/build/meson-linux" "$mb" && meson compile -C "$mb/build/meson-linux"
meson setup "$mb/build/meson-cpp" "$mb" -Dguest_cpp=true && meson compile -C "$mb/build/meson-cpp"
```

## The sources

```sh
git submodule update --init
git -C extern/cemu submodule update --init --depth 1 \
  dependencies/ZArchive dependencies/imgui dependencies/Vulkan-Headers
```

`extern/cemu` is Cemu at a fixed commit. It is never edited by hand: every
change is a numbered patch in `patches/`, applied by
`waterbox/apply-patches.sh` (the build scripts call it). To change Cemu,
edit the tree under `extern/cemu`, then run `waterbox/make-patches.sh`.

## The steps

```sh
waterbox/build-native.sh -m <miniBox> -j 4   # the libraries (build-deps.sh native), Cemu and run-native
waterbox/build-guest.sh -m <miniBox> -j 4    # the same for the guest: build/wbx/core.wbx and run-wbx
waterbox/fetch-devkitpro.sh        # devkitPPC and wut, into build/devkitpro
waterbox/build-homebrew.sh         # the test programs, into build/homebrew
waterbox/build-test-titles.sh      # the frames program as a .wua archive and a .wuhb bundle
waterbox/build-package.sh -m <miniBox> -j 4  # build/package/cemu.chimeraCore
```

- `build-deps.sh native|guest` downloads release archives of zlib, zstd,
  fmt, pugixml, glm, rapidjson, Boost (headers), OpenSSL and curl (headers),
  checks each against its pinned SHA-256, and builds them into
  `build/deps/<flavour>`.
- `build-native.sh` builds `build/native/driver/run-native`; both build
  scripts write the GPU bridge's two halves first (`gen-gl.sh`, into
  `build/generated-gl`).
- `build-guest.sh` builds `build/wbx/core.wbx` (checked by miniBox's
  `check-wbx.sh`) and `build/wbx/run-wbx`, which runs it in the sandbox the
  way Chimera does.
- `build-package.sh` makes the package from `build/wbx/core.wbx` and
  `waterbox/`: `waterbox.config`, `file_slots.json`,
  `default_keybinds.json`, the 26 language tables (`languages/`) and the
  licences (`package-licenses.json`, with `licenses/`). It is written to
  `build/package/cemu.chimeraCore`, the same bytes for the same commit;
  `-r <chimera checkout>` also installs it there, in `build/Cores`.
- `build-test-titles.sh` packs `frames.rpx`, `saves.rpx`, `mii.rpx` and
  `storage.rpx` as titles of this repository's own (title ids
  00050000-F1C3E000, -F1C3E100, -F1C3E200 and -F1C3E300) into `frames.wua`,
  `saves.wua`, `mii.wua` and `storage.wua` with ZArchive's tool (a program
  on its own is not given the console's storage), and `frames.rpx` into
  `frames.wuhb` with wut's `wuhbtool`; and `profiled.wua`, the frames
  program under a published title's id (00050000-10145C00), for the test
  of Cemu's per-game profiles.
- `fetch-devkitpro.sh` takes devkitPPC and wut from the `devkitpro/devkitppc`
  container image's layer (pinned by its digest) without a container tool.
  A devkitPro installation with devkitPPC and wut named by `DEVKITPRO` is
  used instead when there is one.

## The tests

```sh
waterbox/run-gate.sh
```

Each line says what was compared; the script ends with a count of tests
that passed, failed and were skipped, and exits with an error if any
failed. A skipped test is not a pass. The tests run the homebrew programs
and need no game:

- `clock:repeats`: two native runs of 300 frames are one run (the machine's
  clock, the switches between tasks, the program's own record, the
  console's memory and the picture at every 60th frame);
- `clock:stall`: a 300 ms stall of the host changes nothing, and does when
  the console's time is made to count the host's (the test knob
  `CHIMERA_TEST_HOST_CLOCK`);
- `clock:recompiler`: the same with the recompiler;
- `sandbox:native`, `sandbox:recompiler`: the sandbox gives the native
  run's lines, picture included;
- `input:gamepad`: every GamePad button, the sticks and the touch screen
  reach the console as a script pressed them (the `pad` program);
- `input:screens`: the picture of the TV alone and of the GamePad alone,
  and where a touch lands on each;
- `sound:tv`: the TV's sound comes at 48 kHz (480480 pairs in 600 frames,
  within one block of the sound library) and follows the input;
- `lag:counted`: frames in which the program did not read the GamePad are
  counted, and only those;
- `input:sandbox`: the sandbox gives the native run's lines for the `pad`
  program with the script;
- `picture:remade`: a new GPU context before every frame changes nothing
  (the renderer is made again), and does when the renderer is not told
  (the test knob `CHIMERA_TEST_NO_REMAKE`);
- `states:rerecord`, `states:input`: a state saved and loaded before every
  frame changes nothing, for both programs;
- `states:session`: a state carried into another process ends with the
  straight run, picture included;
- `fonts:standin`: with no font given, the console has Cemu's stand-in
  (the default), natively and in the sandbox; with the console's fonts
  asked for and none given, the core refuses to start;
- `firmware:fonts`: the four fonts, given as firmware, reach the console
  byte for byte, both Cemu's copies and the console's own, and Cemu's
  copies given for the console's own are refused (needs `CEMU_FONTS`, a
  folder with Cemu's CafeCn.ttf, CafeKr.ttf, CafeStd.ttf and CafeTw.ttf,
  and for the console's own `CEMU_CONSOLE_FONTS`; skipped without them);
- `formats:titles`: the program as a .wua archive and as a .wuhb bundle is
  mounted as a title and runs as the .rpx does; a file that is neither is
  refused;
- `gx2:draw`: the `draw` program's GX2 drawing (a triangle, squares drawn
  as QUADS with and without an index buffer) is what was drawn, the same
  twice;
- `gx2:queries`: its occlusion query answers the triangle's area with the
  card's count, and 0x100000 on fake;
- `gx2:remade`: a new GPU context before every frame changes nothing;
- `gx2:sandbox`: the sandbox draws the native run's pictures, with states;
- `gx2:readback`: what the GPU drew into a buffer in linear layout reaches
  the console's memory, read by the CPU the same frame; the sandbox the
  same, with states;
- `cpu:cores`: threads pinned to the console's three cores run there and
  take turns, the same every run with either CPU mode; the sandbox the
  same, with states;
- `savedata:round`: the console's saved games exported, and imported into
  another run from a zip; a file that is no zip refused; the sandbox the
  same to the last byte of the console's memory;
- `profiles:shipped`: Cemu's per-game profiles reach the machine (the
  frames program under a profiled title's id runs with that profile);
- `host:random`: no object of the sandboxed core uses the host's random
  numbers (`std::random_device`);
- `firmware:mii`: the console's Mii resource files reach the console byte
  for byte, none with none, a missing one refused (needs `CEMU_MII`, a
  folder with the four files; skipped without it);
- `storage:defaults`: the console's storage holds what upstream's window
  makes in a new one: the Mii Maker's database folders (a file can be made
  in each), the folders titles are installed in, and the lists of
  languages and countries as upstream writes them;
- `nus:refused`: NUS zips that cannot be read are refused with their
  reason (no game needed);
- `nus:game`: a game in the NUS form runs from its zip, twice the same, in
  the sandbox the same, and a wrong key is refused (needs `CEMU_NUS_GAME`
  and `CEMU_COMMON_KEY`; skipped without them);
- `keys:absent`: none of the Nintendo keys upstream builds in (read from
  upstream's own source) is in the core, the native runner, any file of the
  package or any file of this repository;
- `texts:names`: names are written as they are in all 26 language tables
  (`waterbox/tests/check-names.py`);
- `texts:chimera`: Chimera's `tools/check-core-texts.py` on a copy of this
  repository's own files: every key the code and the declarations name is
  in the tables, and every key of the tables is named (needs `CHIMERA_ROOT`,
  a Chimera checkout).

`CEMU_GATE_FRAMES` changes the number of frames of the first tests. About
twenty-two minutes.

The package in Chimera's engine, through projects made and run by
Chimera's headless runner, held to the core's own sandbox runner:

```sh
waterbox/run-frontend.sh --chimera-root <chimera checkout>
```

- `engine:program`: 300 frames of a project leave the runner's MEM2 and
  picture;
- `window:program`: the same project opens in Chimera's window, run
  headless, and the machine a Lua script of the window seeks to, frame 300,
  is chimera-run's (needs the window built in the Chimera checkout and
  `CEMU_QT_LIB`, the lib folder of the Qt 6.12 it was built with; skipped
  without them);
- `engine:input`: a movie pressing every control leaves the runner's MEM2,
  picture and lag frames for the same script (the package's control order is
  the core's);
- `engine:states`: a state around every frame, and a state carried into
  another process, end with the straight run;
- `engine:standin`: a project naming no fonts value gets the stand-in, and
  one that wants the console's fonts and is given none is refused;
- `engine:fonts`: Cemu's copies and the console's fonts as a project's
  firmware reach the console, and an older project whose "console" value
  holds Cemu's copies is refused (needs `CEMU_FONTS`, and for the console's
  own `CEMU_CONSOLE_FONTS`);
- `engine:archive`: a .wua as a project's game;
- `engine:savedata`: Export Save Data's files, and a project's Save Data
  slot read at start;
- `engine:history`: the state history kept by one process and sought into
  by another, as reopening a project does;
- `engine:readback`: the readback program through a project leaves the
  sandbox runner's memory;
- `engine:mii`: the Mii resource as a project's firmware reaches the
  console; a project that wants it and has none is refused (`CEMU_MII`);
- `engine:nus`: a NUS game with its add-on content through a project: the
  sandbox runner's memory, its history across processes, no key and a
  wrong key refused (needs `CEMU_NUS_GAME`, `CEMU_NUS_DLC`,
  `CEMU_COMMON_KEY`; skipped without them).

Games, add-on content, the console's files and keys are never in this
repository, and never on a CI runner. The tests that need them are given
them by the variables named above; without one they say SKIP.

On Windows, from WSL (a chimera-run.exe of the same Chimera, cross-built,
with miniBox's host DLL beside it, and libzstd.dll beside it or in the
Chimera checkout's build/meson-windows; the engine writes a history
uncompressed without it):

```sh
waterbox/run-windows.sh --chimera-root <chimera> --stage /mnt/c/Users/<you>/AppData/Local/Temp/chimera-cemu --exe <chimera-run.exe>
```

It holds the console's memory to Linux's for the test programs, with each
CPU mode (`--cpu` names one), says whether the pictures differ, and runs
the pad program's savestates on Windows (around every frame, carried into
another process, and loaded on Linux) and its state history (kept by one
Windows process, sought into by another and on Linux). About ten minutes.
The stage folder is removed at the end.

To see why a Windows run dies, MSYS2's `gdb.exe` can run chimera-run.exe
and stop at every access violation (`handle SIGSEGV stop print pass`; with
`noprint` it does not stop at all). Most are miniBox's own: the first
write to a clean page.

Chimera's own contract tests of a package:
`<chimera>/build/meson-linux/source/ui/test-packages build/package`.
The texts: `python3 waterbox/tests/check-names.py` (names written as they
are in every language) and `<chimera>/tools/check-core-texts.py .` (every
key the code names is in the English table, and every key is used).

## Running a program by hand

```sh
build/native/driver/run-native build/homebrew/frames.rpx --work build/run --frames 300 --every 60 --screenshot 300=build/run.tga
build/native/driver/run-native build/homebrew/pad.rpx --work build/run --frames 180 --input pad.input --set screens=gamepad
build/wbx/run-wbx build/wbx/core.wbx build/homebrew/frames.rpx --work build/run-wbx --frames 300 --every 60
```

The options, the same for both, are listed at the top of
`waterbox/gate-harness.h`: `--cpu recompiler`, `--input <file>` (lines
"frame mask axes...", the core's order), `--set <setting>=<value>`, `--log`
(Cemu's log on standard error), `--dump-ram <file>` (the console's memory; two dumps are
compared with `waterbox/tests/diff-ram.py`), and for the sandbox
`--rerecord`, `--save-state` and `--state`. A game in the NUS form takes
`--key <file>` (the common key) and `--dlc <zip>` (its add-on content);
the harness's CPU mode is the interpreter unless `--cpu recompiler` says
otherwise (a project's is the recompiler). Two settings no project shows
are there for looking into a game: `--set log_types=<hex>` (Cemu's other
log types, a mask of its LogType bits, with `--log`) and `--set
stack_report=true` (how deep every stack has gone, every 600 frames).
`waterbox/tests/tga2png.py` turns a screenshot into a PNG.
`run-native --extract-nus <zip> <key file> <folder>` writes out a NUS
title's files, decrypted (for looking into system titles; nothing is run).

## CI

`.github/workflows/chimera.yml` builds Chimera's main branch, miniBox,
the native reference, the sandboxed core, the test programs and the
package on a GitHub runner (Mesa's llvmpipe is its GPU), runs
`run-gate.sh` and `run-frontend.sh`, and Chimera's contract tests, and
publishes the package through Chimera's reusable workflow. A runner has no
game, no console file and no key, so firmware:fonts, firmware:mii,
nus:game, engine:fonts, engine:mii and engine:nus say SKIP there, and
`run-windows.sh` is not run; they run where the files are before a push.
