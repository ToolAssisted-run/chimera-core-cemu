#!/usr/bin/env python3
"""Writes waterbox/cemu-sources.cmake: the source files of Cemu's libraries
that this core builds, read from upstream's own CMakeLists.txt files.

Upstream's build is not used (it wants wxWidgets, SDL, curl, cubeb, X11 and
more); its lists of sources are. Every .c/.cpp a library's CMakeLists.txt
names is taken, whatever its condition, and then the files below are left
out: other systems' code, the renderers this core does not build, and the
parts that only talk to the host (devices, a network, a window). Run it
again after moving the pin and look at the difference.

usage: gen-sources.py <cemu checkout> > waterbox/cemu-sources.cmake
"""
import os
import re
import sys

LIBS = [
    # (CMake variable, directory under src/)
    ("CEMU_COMMON_SOURCES", "Common"),
    ("CEMU_CAFE_SOURCES", "Cafe"),
    ("CEMU_COMPONENTS_SOURCES", "Cemu"),
    ("CEMU_CONFIG_SOURCES", "config"),
    ("CEMU_UTIL_SOURCES", "util"),
    ("CEMU_INPUT_SOURCES", "input"),
    ("CEMU_AUDIO_SOURCES", "audio"),
    ("CEMU_IMGUI_SOURCES", "imgui"),
    ("CEMU_RESOURCE_SOURCES", "resource"),
]

# Left out, with the reason. Patterns are matched against the path under src/.
LEFT_OUT = [
    (r"/windows/|Win\.cpp$|_win32\.cpp$|XAudio|DirectSound|DirectInput|XInput", "Windows"),
    (r"Mac\.cpp$|\.mm$|/Metal/|EmitMSL|_macos\.S$", "macOS and Metal"),
    (r"/Vulkan/|imgui_impl_vulkan", "the Vulkan renderer (not built yet)"),
    (r"BackendAArch64|xbyak_aarch64", "ARM hosts"),
    (r"Libusb|libusbWrapper|/GameCube/", "USB devices"),
    (r"/SDL/|/Wiimote/|/DSU/", "the host's controllers (the frontend sends the input)"),
    (r"Cubeb", "the host's sound devices (the frontend takes the sound)"),
    (r"DiscordPresence", "Discord"),
    (r"ExceptionHandler", "the crash handler (a sandbox has no signals)"),
    (r"SystemInfo/SystemInfo(Linux|Unix|Stub)\.cpp", "the host's processor and memory, for the log"),
    (r"Fiber/FiberUnix\.cpp", "fibers on ucontext (waterbox/driver/fiber_chimera.cpp instead)"),
    (r"config/LaunchSettings\.cpp", "the command line (waterbox/driver/launch_settings.cpp instead)"),
]


def sources(src, lib):
    text = open(f"{src}/{lib}/CMakeLists.txt").read()
    text = re.sub(r"#[^\n]*", "", text)
    out = []
    for m in re.finditer(r'"?([A-Za-z0-9_./-]+\.(?:cpp|c|S))\b"?', text):
        path = m.group(1)
        full = os.path.normpath(f"{lib}/{path}")
        if full not in out:
            out.append(full)
    return out


def main():
    root = sys.argv[1]
    src = f"{root}/src"
    print("# Written by waterbox/gen-sources.py from upstream's CMakeLists.txt files.")
    print("# Paths are under extern/cemu/src (dependencies/ is one level up).")
    for var, lib in LIBS:
        files = sources(src, lib)
        kept, dropped = [], []
        for f in files:
            why = next((reason for pat, reason in LEFT_OUT if re.search(pat, f)), None)
            (dropped if why else kept).append((f, why))
        print(f"\nset({var}")
        for f, _ in kept:
            print(f"  {f}")
        print(")")
        for f, why in dropped:
            print(f"# left out: {f} - {why}")


if __name__ == "__main__":
    main()
