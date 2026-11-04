// The core's exports: what the frontend (and both runners of the tests)
// call. The same file in both flavours: natively they are called directly,
// in the sandbox through miniBox. A project names its program in "slots"
// ({"game": ["x.rpx"]}, mounted under that name); the settings are the JSON
// object in "settings". Mounted files in the sandbox, files in the working
// directory natively.
// SPDX-License-Identifier: MIT
#include "cemu_driver.h"
#include "chimera_input.h"
#include "chimera_savedata.h"
#include "gl-bridge.h"
#include "waterbox_settings.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <algorithm>
#include <vector>

#ifdef CHIMERA_GUEST
#include <emulibc.h>
#else
#define ECL_EXPORT __attribute__((visibility("default")))
#define ECL_INVISIBLE
#endif

namespace
{
	std::string g_error;
	bool g_bridge = false;
	uint64_t g_inputPoll ECL_INVISIBLE = 0;

	std::string ReadFile(const char* name)
	{
		std::string out;
		if (FILE* f = fopen(name, "rb"))
		{
			char buf[4096];
			size_t n;
			while ((n = fread(buf, 1, sizeof buf, f)) > 0)
				out.append(buf, n);
			fclose(f);
		}
		return out;
	}

	bool Exists(const std::string& name)
	{
		if (FILE* f = fopen(name.c_str(), "rb"))
		{
			fclose(f);
			return true;
		}
		return false;
	}

	// a file's size in bytes, or -1 when it cannot be opened
	long long FileSize(const std::string& name)
	{
		FILE* f = fopen(name.c_str(), "rb");
		if (!f)
			return -1;
		long long size = -1;
		if (fseek(f, 0, SEEK_END) == 0)
			size = ftell(f);
		fclose(f);
		return size;
	}

	// The four fonts as upstream Cemu ships them (resources/sharedFonts), by
	// size: other files than the console's own, and of other sizes, all four
	// (waterbox.config declares both). A project made before Console Fonts had
	// three values pinned Cemu's copies under "console", which meant them then;
	// the console's own value refuses them (owner, 2026-11-04: refused, not
	// converted).
	struct CemuFont
	{
		const char* name;
		long long size;
	};
	constexpr CemuFont kCemuFonts[] = {
		{"CafeCn.ttf", 9664448}, {"CafeKr.ttf", 5626464}, {"CafeStd.ttf", 5363768}, {"CafeTw.ttf", 6593548},
	};

	// the common key's firmware id: the name it is mounted under
	constexpr const char* kCommonKey = "wiiu-common-key.bin";
	// the console's Mii resource files: firmware, mounted under their names
	constexpr const char* kMiiResource[] = {"FFLResHigh.dat", "FFLResHighLG.dat", "FFLResMiddle.dat", "FFLResMiddleLG.dat"};

	// The first name in a slot of the project's slot map, or "".
	std::string SlotFirst(const std::string& slots, const char* id)
	{
		const std::string k = std::string("\"") + id + "\"";
		const auto at = slots.find(k);
		if (at == std::string::npos)
			return "";
		size_t i = slots.find('[', at + k.size());
		if (i == std::string::npos)
			return "";
		i = slots.find('"', i);
		if (i == std::string::npos)
			return "";
		std::string name;
		for (size_t j = i + 1; j < slots.size() && slots[j] != '"'; j++)
		{
			if (slots[j] == '\\' && j + 1 < slots.size())
				j++;
			name += slots[j];
		}
		return name;
	}

	// A mounted file's name as it opens: a project mounts a slot's file under
	// the name the map gives, a file opened on its own sits at "/<name>" too.
	std::string Mounted(const std::string& name)
	{
		if (name.empty() || name[0] == '/' || Exists(name))
			return name;
		if (Exists("/" + name))
			return "/" + name;
		return name;
	}
}

void chimera_input_polled()
{
	if (g_inputPoll)
		reinterpret_cast<uintptr_t (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t)>(
			static_cast<uintptr_t>(g_inputPoll))(0, 0, 0, 0, 0, 0);
}

extern "C"
{
	ECL_EXPORT const char* GetLoadError(void)
	{
		return g_error.c_str();
	}

	// The GPU bridge: the host's dispatcher, handed over before Init. Cemu's
	// OpenGL renderer draws on the machine's GPU through it.
	ECL_EXPORT void SetGpuBridge(uint64_t addr)
	{
		g_bridge = chimera_gl_install(reinterpret_cast<chimera_gl_bridge_fn>(static_cast<uintptr_t>(addr)));
	}

	ECL_EXPORT int Init(void)
	{
		const std::string slots = ReadFile("slots");
		std::string name = Mounted(SlotFirst(slots, "game"));
		if (name.empty())
		{
			name = ReadFile("rom.name");
			while (!name.empty() && (name.back() == '\n' || name.back() == '\r'))
				name.pop_back();
		}
		if (name.empty())
		{
			g_error = "error.no_game";
			return 0;
		}
		if (!g_bridge)
		{
			g_error = "error.no_gpu";
			return 0;
		}
		chimera::BootOptions options;
		options.game = name;
		// the save data slot: one zip, as Export Save Data wrote it
		const std::string save = SlotFirst(slots, "savedata");
		if (!save.empty())
		{
			options.savedata = Mounted(save);
			options.savedataName = save;
		}
		// a NUS game's add-on content: one zip of its NUS folder
		const std::string dlc = SlotFirst(slots, "dlc");
		if (!dlc.empty())
		{
			options.dlc = Mounted(dlc);
			options.dlcName = dlc;
		}
		// the console's common key, which a NUS game needs: firmware, 16 bytes
		const std::string key = Mounted(kCommonKey);
		if (Exists(key))
		{
			const std::string bytes = ReadFile(key.c_str());
			if (bytes.size() != 16)
			{
				g_error = std::string("error.key_not_a_key\x1f") + kCommonKey;
				return 0;
			}
			options.commonKey.assign(bytes.begin(), bytes.end());
		}
		char value[64];
		if (wbx_setting_str("cpu_mode", value, sizeof value) < 0 || !strcmp(value, "recompiler"))
			options.recompiler = true;
		else if (!strcmp(value, "interpreter"))
			options.recompiler = false;
		else
		{
			g_error = std::string("setting.cpu_mode.unknown\x1f") + value;
			return 0;
		}
		options.clockStart = wbx_setting_long("clock_start", options.clockStart);
		options.log = wbx_setting_bool("log", 0) != 0;
		options.stackReport = wbx_setting_bool("stack_report", 0) != 0;
		if (wbx_setting_str("log_types", value, sizeof value) >= 0)
			options.logTypes = strtoull(value, nullptr, 16);
		static const char* const kLanguages[] = {"ja", "en", "fr", "de", "it", "es", "zh", "ko", "nl", "pt", "ru", "tw"};
		if (wbx_setting_str("console_language", value, sizeof value) >= 0)
		{
			options.consoleLanguage = -1;
			for (int i = 0; i < (int)(sizeof kLanguages / sizeof kLanguages[0]); i++)
				if (!strcmp(value, kLanguages[i]))
					options.consoleLanguage = i;
			if (options.consoleLanguage < 0)
			{
				g_error = std::string("setting.console_language.unknown\x1f") + value;
				return 0;
			}
		}
		if (wbx_setting_str("screens", value, sizeof value) < 0 || !strcmp(value, "both"))
			chimera::SetScreens(chimera::Screens::Both);
		else if (!strcmp(value, "tv"))
			chimera::SetScreens(chimera::Screens::TV);
		else if (!strcmp(value, "gamepad"))
			chimera::SetScreens(chimera::Screens::GamePad);
		else
		{
			g_error = std::string("setting.screens.unknown\x1f") + value;
			return 0;
		}
		// the renderer: Cemu's OpenGL on the host's card, through the GPU
		// bridge, the one there is (its value ends in -hw, which is what makes
		// Chimera's window give the bridge)
		if (wbx_setting_str("renderer", value, sizeof value) >= 0 && strcmp(value, "opengl-hw") != 0)
		{
			g_error = std::string("setting.renderer.unknown\x1f") + value;
			return 0;
		}
		if (wbx_setting_str("occlusion_queries", value, sizeof value) < 0 || !strcmp(value, "card"))
			options.fakeOcclusion = false;
		else if (!strcmp(value, "fake"))
			options.fakeOcclusion = true;
		else
		{
			g_error = std::string("setting.occlusion_queries.unknown\x1f") + value;
			return 0;
		}
		// The console's fonts: firmware, each mounted under its name - Cemu's
		// copies or the console's own, pinned apart by the package and mounted
		// alike, except that the console's own refuses Cemu's copies (an older
		// project's). With "standin" (the default) Cemu's own stand-in is given
		// to games instead.
		if (wbx_setting_str("console_fonts", value, sizeof value) < 0 || !strcmp(value, "standin"))
			;
		else if (!strcmp(value, "cemu") || !strcmp(value, "console"))
		{
			for (const char* font : {"CafeCn.ttf", "CafeKr.ttf", "CafeStd.ttf", "CafeTw.ttf"})
			{
				const std::string mounted = Mounted(font);
				if (!Exists(mounted))
				{
					g_error = std::string("error.no_font\x1f") + font;
					return 0;
				}
				if (!strcmp(value, "console"))
					for (const CemuFont& cemu : kCemuFonts)
						if (!strcmp(cemu.name, font) && FileSize(mounted) == cemu.size)
						{
							g_error = std::string("error.font_is_cemu_copy\x1f") + font;
							return 0;
						}
				options.fonts.push_back(mounted);
			}
		}
		else
		{
			g_error = std::string("setting.console_fonts.unknown\x1f") + value;
			return 0;
		}
		// The console's Mii resource: firmware, the four files of its system
		// title, each mounted under its name. With "none" there is none, as
		// on a console without it.
		if (wbx_setting_str("mii_resource", value, sizeof value) < 0 || !strcmp(value, "console"))
		{
			for (const char* file : kMiiResource)
			{
				const std::string mounted = Mounted(file);
				if (!Exists(mounted))
				{
					g_error = std::string("error.no_mii_resource\x1f") + file;
					return 0;
				}
				options.miiResource.emplace_back(file, mounted);
			}
		}
		else if (strcmp(value, "none") != 0)
		{
			g_error = std::string("setting.mii_resource.unknown\x1f") + value;
			return 0;
		}
		g_error = chimera::Boot(options);
		return g_error.empty() ? 1 : 0;
	}

	// Save data out (the frontend's Export Save Data): a snapshot of the
	// console's saved games, then each file by index.
	ECL_EXPORT int32_t GetSaveDataFileCount(void)
	{
		return (int32_t)chimera::SaveDataSnapshot(chimera::StoragePath());
	}
	ECL_EXPORT const char* GetSaveDataFileName(int32_t i)
	{
		return chimera::SaveDataName((size_t)i).c_str();
	}
	ECL_EXPORT int64_t GetSaveDataFileSize(int32_t i)
	{
		return (int64_t)chimera::SaveDataBytes((size_t)i).size();
	}
	ECL_EXPORT const uint8_t* GetSaveDataFileBuffer(int32_t i)
	{
		return chimera::SaveDataBytes((size_t)i).data();
	}

	// The GamePad's axes, each set before the frame it is held in.
	ECL_EXPORT void SetAxis(int32_t index, int32_t value)
	{
		chimera::SetPadAxis(index, value);
	}

	ECL_EXPORT void FrameAdvance(uint64_t buttons)
	{
		chimera::SetPadButtons(buttons);
		chimera::BeginFrameInput();
		chimera::BeginFrameAudio();
		chimera::RunFrame();
	}

	// a frame in which the console did not read the GamePad is a lag frame
	ECL_EXPORT int InputWasRead(void)
	{
		return chimera::InputWasRead() ? 1 : 0;
	}

	// Called where the console reads the GamePad, for a script's
	// event.oninputpoll. The slot's address is the same in every session and
	// no state carries it.
	ECL_EXPORT void SetInputPollCallback(uint64_t fn)
	{
		g_inputPoll = fn;
	}

	ECL_EXPORT void SetRenderingEnabled(int on)
	{
		chimera::SetRendering(on != 0);
	}

	ECL_EXPORT uint32_t* GetVideoBgra(void)
	{
		return const_cast<uint32_t*>(chimera::Picture());
	}
	ECL_EXPORT int GetVideoWidth(void)
	{
		return chimera::PictureWidth();
	}
	ECL_EXPORT int GetVideoHeight(void)
	{
		return chimera::PictureHeight();
	}
	ECL_EXPORT int GetVsyncNumerator(void)
	{
		return 60000;
	}
	ECL_EXPORT int GetVsyncDenominator(void)
	{
		return 1001;
	}

	// the sound of the last frame: pairs at 48 kHz
	ECL_EXPORT int16_t* GetAudio(void)
	{
		int pairs;
		return const_cast<int16_t*>(chimera::FrameAudio(pairs));
	}
	ECL_EXPORT int GetAudioSampleCount(void)
	{
		int pairs;
		chimera::FrameAudio(pairs);
		return pairs;
	}

	// The console's memory: MEM2 and MEM1, and the whole address space as a
	// bus. The console's words are big-endian.
	ECL_EXPORT int GetMemoryDomainCount(void)
	{
		return chimera::DomainCount();
	}
	ECL_EXPORT const char* GetMemoryDomainName(int32_t i)
	{
		return chimera::DomainName(i);
	}
	ECL_EXPORT uint8_t* GetMemoryDomainPtr(int32_t i)
	{
		return chimera::DomainPointer(i);
	}
	ECL_EXPORT int64_t GetMemoryDomainSize(int32_t i)
	{
		return chimera::DomainSize(i);
	}
	ECL_EXPORT int GetMemoryDomainWritable(int32_t)
	{
		return 1;
	}
	ECL_EXPORT int32_t GetBusCount(void)
	{
		return 1;
	}
	ECL_EXPORT const char* GetBusName(int32_t)
	{
		return "System Bus";
	}
	ECL_EXPORT int64_t GetBusSize(int32_t)
	{
		return int64_t(1) << 32;
	}
	ECL_EXPORT int32_t GetBusWritable(int32_t)
	{
		return 1;
	}
	// (the engine passes a bus address as 32 bits: past 2 GiB it is negative)
	ECL_EXPORT int32_t PeekBus(int32_t, int32_t address)
	{
		return chimera::BusPeek((uint32_t)address);
	}
	ECL_EXPORT void PokeBus(int32_t, int32_t address, int32_t value)
	{
		chimera::BusPoke((uint32_t)address, (uint8_t)value);
	}
	// up to 64 KiB at once, in memory no state carries
	ECL_EXPORT const uint8_t* ReadBus(int32_t, int64_t address, int32_t len)
	{
		static uint8_t buffer[65536] ECL_INVISIBLE;
		if (len < 0)
			len = 0;
		if (len > (int32_t)sizeof buffer)
			len = sizeof buffer;
		chimera::BusRead((uint64_t)address, buffer, (size_t)len);
		return buffer;
	}
	// the mapped ranges, as they are now
	ECL_EXPORT const int64_t* GetBusRanges(int32_t)
	{
		static int64_t out[256] ECL_INVISIBLE;
		std::vector<int64_t> ranges;
		chimera::BusRanges(ranges);
		const size_t n = std::min<size_t>(ranges.size(), 254);
		memcpy(out, ranges.data(), n * sizeof(int64_t));
		out[n] = 0;
		out[n + 1] = 0;
		return out;
	}

	ECL_EXPORT uint64_t GetFrameCount(void)
	{
		return chimera::FrameCount();
	}

	// the tests': the machine's clock, the switches between its tasks, the
	// test programs' record, a digest and a map of the console's memory
	ECL_EXPORT uint64_t GetClockCycles(void)
	{
		return chimera::ClockCycles();
	}
	ECL_EXPORT uint64_t GetTaskSwitches(void)
	{
		return chimera::TaskSwitches();
	}
	ECL_EXPORT const uint8_t* GetMailbox(void)
	{
		return chimera::FindMailbox();
	}
	ECL_EXPORT uint64_t GetRamDigest(void)
	{
		return chimera::MemoryDigest();
	}
	ECL_EXPORT const uint64_t* GetMemoryRanges(void)
	{
		return chimera::MemoryRanges();
	}
}
