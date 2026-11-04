# chimera-core-cemu

[Cemu](https://github.com/cemu-project/Cemu)'s Wii U emulator as a
[Chimera](https://github.com/ToolAssisted-run/chimera) core.

## Using it in Chimera

Chimera includes no cores and downloads none. Download `cemu.chimeraCore`
from this repository's
[Releases](https://github.com/ToolAssisted-run/chimera-core-cemu/releases)
page, or build it, and put it in the `cores` folder beside Chimera's program.
Tools > Core Manager... lists what is in that folder. The same file works on
Linux and on Windows. The core draws with Cemu's OpenGL renderer on the
computer's graphics card.

## What you bring

- A game you own: a Wii U archive (.wua; Cemu makes one from a game's
  installed folders, with its update and add-on content), a zip of its NUS
  folder (the encrypted .app files with title.tmd, title.tik and
  title.cert, made without compression; its add-on content's NUS zip goes in
  a slot of its own), a homebrew bundle (.wuhb) or a program (.rpx). Disc
  images (.wud, .wux) and installed folders are not taken.
- For a NUS zip, the Wii U common key, as a file of its 16 bytes, given as
  firmware. The core carries no Nintendo key; a .wua needs none.
- The console's Mii resource, the four files of its system title
  0005001b-10056000, given as firmware (the Mii Resource setting's default).
  The setting can go without them, and a game that draws Miis may then stop:
  New Super Mario Bros. U does, after its title screen.
- Optionally, the console's four shared fonts, as firmware: Cemu's copies or
  the console's own (system title 0005001b-10042400). Without them, the
  default, games are given Cemu's free stand-in font (Lato).

A project can start from saved games, as Export Save Data writes them.

## Status

The core runs a frame at a time on the machine's own clock, the same way
every time, in Chimera's engine and in its window: both screens, the
GamePad's controls, the TV's sound, savestates in the same process and in
another, and the state history (greenzone) kept across sessions. Its tests
run on this repository's own Wii U test programs.

One game has been tried, New Super Mario Bros. U: played from boot through
the opening scene and the world map into its first level, on Linux with
software OpenGL (Mesa's llvmpipe), and its history over that level sought
into on Windows on an NVIDIA GTX 1060. On that computer the published
core also played it in Chimera's window, with TAStudio open, to the notice
after its title screen. No other game, no other level of it, no AMD or
Intel card and no macOS have been tried. [docs/PLAN.md](docs/PLAN.md) says
what was measured.

## Building and testing

[docs/BUILDING.md](docs/BUILDING.md) has every step, and
[AGENTS.md](AGENTS.md) is the guide for an AI coding agent. Cemu is a
submodule at a fixed commit (`extern/cemu`) with the numbered patches in
`patches/` applied to it, and Nintendo's keys taken out of it as it is
built.

## Licence

This repository's own code is MIT. Cemu is MPL-2.0; the changes to it in
`patches/` are offered under the MPL. See [LICENSE](LICENSE).
