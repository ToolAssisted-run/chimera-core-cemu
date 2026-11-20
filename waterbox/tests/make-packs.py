#!/usr/bin/env python3
# The graphic packs of the tests (run-gate.sh packs:*, run-frontend.sh
# engine:packs): packs of this repository's own, written when the tests run,
# for the frames program (tests/homebrew/frames). No pack of the community's
# is used or kept.
#
#   make-packs.py <frames.elf> <objdump> <out folder>
#
# The frames program counts its alarm's calls in its mailbox (alarm_count,
# the sixth value). Its alarm_callback adds 1 to it with one instruction,
# "addi rN,rN,1", found by disassembling the program (its address is in the
# program's code, which the patch names by the program's own addresses).
#
# out/alarm.zip holds two pack folders:
#   chimera-alarm, for the frames title (00050000-F1C3E000): a patch that
#     makes that instruction add $step, with three presets of $step - the
#     [Default] section's 1, "Two" (2, the first preset) and "Three" (3,
#     marked default = 1, which upstream chooses when the user has chosen
#     none) - and an output shader that turns the picture's colours over
#     (255 - each channel); rules.txt and output.glsl are deflated, the
#     patch is stored, so both ways of reading a zip's entry are used;
#   chimera-other, for another title (00050000-10145C00): the same patch
#     with a step of 100, which the frames title must not get.
# out/choices.zip holds chimera-choices, for the frames title, a pack whose
#   presets are in two categories (the Graphic Packs choices of the project,
#   docs/PLAN.md "Choosing a pack's presets"): "Count" (Two: $step 2; Five:
#   $step 5, marked default = 1) and "Picture" (Plain: $invert 0, the first;
#   Inverted: $invert 1), with the same patch and an output shader that
#   turns the colours over when $invert is 1. Without a choice: the alarm
#   counts by 5 and the picture is the game's own.
# out/other.zip holds chimera-other alone; out/norules.zip a folder with no
# rules.txt; out/notzip.zip is not a zip.
import re
import subprocess
import sys
import zipfile

FRAMES_TITLE = "00050000F1C3E000"
OTHER_TITLE = "0005000010145C00"


def find_addi(elf, objdump):
    out = subprocess.run([objdump, "-d", "--disassemble=alarm_callback", elf], check=True,
                         capture_output=True, text=True).stdout
    for line in out.splitlines():
        m = re.match(r"\s*([0-9a-f]+):\s+(?:[0-9a-f]{2} ){4}\s*addi\s+(r\d+),(r\d+),1$", line)
        if m and m.group(2) == m.group(3):
            return int(m.group(1), 16), m.group(2)
    sys.exit("no 'addi rN,rN,1' in alarm_callback of " + elf)


def rules(title, name, presets):
    text = ("[Definition]\n"
            f"titleIds = {title}\n"
            f"name = {name}\n"
            f"path = \"Chimera/Tests/{name}\"\n"
            "description = A pack of the Cemu core's tests.\n"
            "version = 6\n")
    return text + presets


def patch(addr, reg, step):
    return (f"[ChimeraAlarm]\n"
            "moduleMatches = rpx\n"
            f"0x{addr:08x} = addi {reg}, {reg}, {step}\n")


INVERT = """
void outputShader()
{
	colorOut0 = vec4(vec3(1.0) - texture(textureSrc, passUV).rgb, 1.0);
}
"""

ALARM_PRESETS = """
[Default]
$step:int = 1

[Preset]
name = Two
$step:int = 2

[Preset]
name = Three
$step:int = 3
default = 1
"""


CHOICES_PRESETS = """
[Default]
$step:int = 1
$invert:int = 0

[Preset]
name = Two
category = Count
$step:int = 2

[Preset]
name = Five
category = Count
default = 1
$step:int = 5

[Preset]
name = Plain
category = Picture
$invert:int = 0

[Preset]
name = Inverted
category = Picture
$invert:int = 1
"""

CHOICES_SHADER = """
void outputShader()
{
	vec3 c = texture(textureSrc, passUV).rgb;
	colorOut0 = vec4($invert == 1 ? vec3(1.0) - c : c, 1.0);
}
"""


def main():
    if len(sys.argv) != 4:
        sys.exit("usage: make-packs.py <frames.elf> <objdump> <out folder>")
    elf, objdump, out = sys.argv[1:]
    addr, reg = find_addi(elf, objdump)
    D, S = zipfile.ZIP_DEFLATED, zipfile.ZIP_STORED

    def other(z):
        z.writestr("chimera-other/rules.txt", rules(OTHER_TITLE, "Other", ""), D)
        z.writestr("chimera-other/patch_alarm.asm", patch(addr, reg, 100), S)

    with zipfile.ZipFile(out + "/alarm.zip", "w") as z:
        z.writestr("chimera-alarm/", "")
        z.writestr("chimera-alarm/rules.txt", rules(FRAMES_TITLE, "Alarm", ALARM_PRESETS), D)
        z.writestr("chimera-alarm/patch_alarm.asm", patch(addr, reg, "$step"), S)
        z.writestr("chimera-alarm/output.glsl", INVERT, D)
        other(z)
    with zipfile.ZipFile(out + "/choices.zip", "w") as z:
        z.writestr("chimera-choices/rules.txt", rules(FRAMES_TITLE, "Choices", CHOICES_PRESETS), D)
        z.writestr("chimera-choices/patch_alarm.asm", patch(addr, reg, "$step"), S)
        z.writestr("chimera-choices/output.glsl", CHOICES_SHADER, D)
    with zipfile.ZipFile(out + "/other.zip", "w") as z:
        other(z)
    with zipfile.ZipFile(out + "/norules.zip", "w") as z:
        z.writestr("chimera-alarm/readme.txt", "no rules.txt here\n", D)
    with open(out + "/notzip.zip", "w") as f:
        f.write("not a zip\n")
    print(f"alarm_callback's addi {reg},{reg},1 at 0x{addr:08x}")


main()
