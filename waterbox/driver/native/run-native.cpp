// run-native: the core's exports called directly, the reference the sandbox
// is held to. The same harness as run-wbx (gate-harness.h), the same lines;
// the program and the files a project mounts are in the work directory,
// and the GPU bridge's host half answers in this process exactly as it does
// for the sandbox, so that both draw through the same calls.
//
// usage: run-native <program> --work <dir> [options: see gate-harness.h]
// SPDX-License-Identifier: MIT
#include "gate-harness.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace chimera
{
	std::string ExtractNus(const std::string& zip, const std::vector<uint8_t>& key, const std::string& out); // cemu_driver.cpp
}

extern "C"
{
	int chimera_gl_host_init(char* err, int errlen);
	const char* chimera_gl_host_description(void);
	uintptr_t chimera_gl_host_dispatch(uintptr_t op, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e);
	unsigned long chimera_gl_host_unhandled(long* last_op);
	void chimera_gl_host_state_loaded(void);

	// the core's exports (wbx-entry.cpp)
	const char* GetLoadError(void);
	void SetGpuBridge(uint64_t addr);
	int Init(void);
	void FrameAdvance(uint64_t buttons);
	uint32_t* GetVideoBgra(void);
	int GetVideoWidth(void);
	int GetVideoHeight(void);
	uint64_t GetFrameCount(void);
	uint64_t GetClockCycles(void);
	uint64_t GetTaskSwitches(void);
	const uint8_t* GetMailbox(void);
	uint64_t GetRamDigest(void);
	const uint64_t* GetMemoryRanges(void);
	void SetAxis(int32_t index, int32_t value);
	int16_t* GetAudio(void);
	int GetAudioSampleCount(void);
	int InputWasRead(void);
	int32_t GetSaveDataFileCount(void);
	const char* GetSaveDataFileName(int32_t i);
	int64_t GetSaveDataFileSize(int32_t i);
	const uint8_t* GetSaveDataFileBuffer(int32_t i);
}

static bool WriteFile(const std::string& path, const std::string& text)
{
	FILE* f = fopen(path.c_str(), "wb");
	if (!f)
		return false;
	fwrite(text.data(), 1, text.size(), f);
	return fclose(f) == 0;
}

int main(int argc, char** argv)
{
	struct harness_opts o;
	// run-native --extract-nus <zip> <key file> <folder>: a title's files,
	// decrypted, for looking into (system titles); nothing is run
	if (argc == 5 && !strcmp(argv[1], "--extract-nus"))
	{
		std::vector<uint8_t> key;
		if (FILE* f = fopen(argv[3], "rb"))
		{
			uint8_t b[17];
			const size_t n = fread(b, 1, sizeof b, f);
			fclose(f);
			key.assign(b, b + n);
		}
		if (key.size() != 16)
		{
			fprintf(stderr, "%s is not a key of 16 bytes\n", argv[3]);
			return 2;
		}
		const std::string error = chimera::ExtractNus(argv[2], key, argv[4]);
		if (!error.empty())
			fprintf(stderr, "cannot extract: %s\n", error.c_str());
		return error.empty() ? 0 : 1;
	}
	if (!harness_parse(argc, argv, &o))
		return 2;
	char program[4096];
	if (!realpath(o.program, program))
	{
		fprintf(stderr, "cannot find %s\n", o.program);
		return 2;
	}
	for (int s = 0; s < o.shots; s++)
	{
		static char shot[HARNESS_MAX_SHOTS][4096];
		if (o.shot_path[s][0] != '/' && getcwd(shot[s], sizeof shot[s]))
		{
			std::string p = std::string(shot[s]) + "/" + o.shot_path[s];
			snprintf(shot[s], sizeof shot[s], "%s", p.c_str());
			o.shot_path[s] = shot[s];
		}
	}
	// the save data zip and where its export goes, found before the work
	// directory is entered
	static char savedata[4096];
	if (o.savedata && !realpath(o.savedata, savedata))
	{
		fprintf(stderr, "cannot find %s\n", o.savedata);
		return 2;
	}
	static std::string savedataOut;
	if (o.savedata_out && o.savedata_out[0] != '/')
	{
		char here[4096];
		if (getcwd(here, sizeof here))
		{
			savedataOut = std::string(here) + "/" + o.savedata_out;
			o.savedata_out = savedataOut.c_str();
		}
	}
	// a NUS game's add-on content and the common key, found before the
	// work directory is entered
	static char dlc[4096], key[4096];
	if (o.dlc && !realpath(o.dlc, dlc))
	{
		fprintf(stderr, "cannot find %s\n", o.dlc);
		return 2;
	}
	if (o.key && !realpath(o.key, key))
	{
		fprintf(stderr, "cannot find %s\n", o.key);
		return 2;
	}
	// the console's Mii resource files that the folder has
	static char mii[HARNESS_MII_FILES][4096];
	bool miiHas[HARNESS_MII_FILES] = {};
	if (o.mii)
		for (size_t i = 0; i < HARNESS_MII_FILES; i++)
		{
			const std::string p = std::string(o.mii) + "/" + harness_mii_names[i];
			miiHas[i] = realpath(p.c_str(), mii[i]) != nullptr;
		}
	// the console's fonts, found before the work directory is entered
	static char fonts[4][4096];
	if (o.fonts)
		for (int i = 0; i < 4; i++)
		{
			const std::string p = std::string(o.fonts) + "/" + harness_font_names[i];
			if (!realpath(p.c_str(), fonts[i]))
			{
				fprintf(stderr, "cannot find %s\n", p.c_str());
				return 2;
			}
		}
	char cwd[4096];
	if (!getcwd(cwd, sizeof cwd))
		return 2;
	static std::string dump;
	if (o.dump_ram && o.dump_ram[0] != '/')
	{
		dump = std::string(cwd) + "/" + o.dump_ram;
		o.dump_ram = dump.c_str();
	}
	if (!harness_fresh_work(o.work) || chdir(o.work) != 0)
		return 2;
	// the files a project mounts: the program under its own name, the slot
	// map naming it, the settings
	const char* base = harness_base(program);
	if (symlink(program, base) != 0)
	{
		fprintf(stderr, "cannot link %s\n", base);
		return 2;
	}
	// firmware is mounted under its id
	if (o.fonts)
		for (int i = 0; i < 4; i++)
			if (symlink(fonts[i], harness_font_names[i]) != 0)
			{
				fprintf(stderr, "cannot link %s\n", harness_font_names[i]);
				return 2;
			}
	if (o.savedata && symlink(savedata, harness_base(savedata)) != 0)
	{
		fprintf(stderr, "cannot link %s\n", savedata);
		return 2;
	}
	for (size_t i = 0; i < HARNESS_MII_FILES; i++)
		if (miiHas[i] && symlink(mii[i], harness_mii_names[i]) != 0)
		{
			fprintf(stderr, "cannot link %s\n", harness_mii_names[i]);
			return 2;
		}
	if (o.dlc && symlink(dlc, harness_base(dlc)) != 0)
	{
		fprintf(stderr, "cannot link %s\n", dlc);
		return 2;
	}
	if (o.key && symlink(key, harness_key_name) != 0)
	{
		fprintf(stderr, "cannot link %s\n", key);
		return 2;
	}
	char buf[4096];
	harness_slots(base, o.savedata ? harness_base(savedata) : nullptr, o.dlc ? harness_base(dlc) : nullptr, buf, sizeof buf);
	WriteFile("slots", buf);
	harness_settings(&o, buf, sizeof buf);
	WriteFile("settings", buf);

	char glerr[256] = "";
	if (chimera_gl_host_init(glerr, sizeof glerr) != 0)
	{
		fprintf(stderr, "gpu bridge: %s\n", glerr);
		return 5;
	}
	fprintf(stderr, "gpu bridge: %s\n", chimera_gl_host_description());
	SetGpuBridge((uint64_t)(uintptr_t)&chimera_gl_host_dispatch);

	const struct harness_core c = {
		Init, GetLoadError, FrameAdvance, GetVideoBgra, GetVideoWidth, GetVideoHeight, GetFrameCount,
		GetClockCycles, GetTaskSwitches, GetRamDigest, GetMailbox, GetMemoryRanges,
		nullptr, nullptr, nullptr, chimera_gl_host_state_loaded,
		SetAxis, GetAudio, GetAudioSampleCount, InputWasRead,
		GetSaveDataFileCount, GetSaveDataFileName, GetSaveDataFileSize, GetSaveDataFileBuffer,
	};
	const int rc = harness_run(&c, &o);
	long last = 0;
	const unsigned long unhandled = chimera_gl_host_unhandled(&last);
	if (unhandled)
		fprintf(stderr, "gpu bridge: %lu calls had no case (last opcode %ld)\n", unhandled, last);
	fflush(stdout);
	_Exit(rc);
}
