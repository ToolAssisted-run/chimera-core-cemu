// The Cemu core's machine: what the core's exports (wbx-entry.cpp) drive,
// the same code natively and in the sandbox.
#pragma once
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace chimera
{
	struct BootOptions
	{
		std::string game;         // the program (.rpx), as the C library opens it
		bool recompiler = false;  // false: the interpreter
		int64_t clockStart = 1495889068; // the console's calendar at power-on, seconds since 1970 (UTC)
		bool log = false;         // Cemu's log on standard error
		int consoleLanguage = 1;  // CafeConsoleLanguage: 1 is English
		// The console's four shared fonts (CafeCn, CafeKr, CafeStd, CafeTw),
		// each as the C library opens it; empty: Cemu's stand-in (Lato)
		std::vector<std::string> fonts;
		// occlusion queries: false, the card's count (upstream's); true, every
		// object reported fully visible
		bool fakeOcclusion = false;
		// the project's save data (a zip of usr/save/... entries), as the C
		// library opens it, and its name; empty: none
		std::string savedata, savedataName;
		// a NUS game (the game is a zip of a title's NUS folder): the zip of
		// its add-on content's NUS folder and its name (empty: none), and the
		// console's common key (16 bytes; empty: none given)
		std::string dlc, dlcName;
		std::vector<uint8_t> commonKey;
		// a measurement, not a setting a project shows: how deep every
		// stack has gone, on standard error every 600 frames
		bool stackReport = false;
		// the console's Mii resource (its system title 0005001b-10056000):
		// each file's name and where the C library opens it; empty: none
		std::vector<std::pair<std::string, std::string>> miiResource;
		// Cemu's log types beside the ones always on (a mask of LogType
		// bits; the hidden setting log_types, in hexadecimal), with log on
		uint64_t logTypes = 0;
	};

	// the console's storage (its mlc01 folder) in the machine's filesystem
	std::string StoragePath();

	// A tool of the native runner's, for looking into a title: every file of
	// a NUS zip's title, decrypted with the key and written under out (a
	// host folder). An empty string, or why not.
	std::string ExtractNus(const std::string& zip, const std::vector<uint8_t>& key, const std::string& out);

	// Starts the console and the program, at time 0 of the machine's clock.
	// Returns an empty string, or why not.
	std::string Boot(const BootOptions& options);

	// Runs the machine to the end of the next frame: two microseconds after
	// the next vertical blank (59.94 Hz, 1001/60000 s apart).
	void RunFrame();
	uint64_t FrameCount();
	uint64_t ClockCycles();   // the machine's clock, Espresso core cycles
	uint64_t TaskSwitches();

	// The picture last shown: the TV's, the GamePad's or both, as the setting
	// says (chimera_input.h), BGRA, top row first: read from the renderer
	// when asked (and when rendering is on).
	constexpr int kTvWidth = 1280, kTvHeight = 720;
	const uint32_t* Picture();
	int PictureWidth();
	int PictureHeight();
	void SetRendering(bool on);

	// The mailbox of the test programs (tests/homebrew), found by its magic in
	// the console's memory; null when there is none.
	const uint8_t* FindMailbox();
	// a digest of the console's mapped memory
	uint64_t MemoryDigest();
	// the console's mapped memory: (console address, size, pointer) triples,
	// ended by a size of 0
	const uint64_t* MemoryRanges();

	// The console's memory, for the frontend's tools: two domains (MEM2 and
	// MEM1, at their console addresses 0x10000000 and 0xF4000000), and the
	// whole 4 GiB address space as a bus, where what is not mapped reads 0.
	int DomainCount();
	const char* DomainName(int i);
	uint8_t* DomainPointer(int i);
	int64_t DomainSize(int i);
	uint8_t BusPeek(uint64_t address);
	void BusPoke(uint64_t address, uint8_t value);
	void BusRead(uint64_t address, uint8_t* out, size_t len);
	void BusRanges(std::vector<int64_t>& out);
}
