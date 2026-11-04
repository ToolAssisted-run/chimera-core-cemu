// The console's saved games, in and out (driver/cemu/savedata.cpp).
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace chimera
{
	// A zip of usr/save/... entries (the export's own form), written into the
	// console's storage (the mlc01 folder, absolute in the machine's
	// filesystem). False, and a text key with its arguments in error, when
	// it cannot be.
	bool LoadSaveData(const std::string& mounted, const std::string& name, const std::string& storage, std::string& error);

	// The saved games now: a snapshot, then each by index.
	size_t SaveDataSnapshot(const std::string& storage);
	const std::string& SaveDataName(size_t index);
	const std::vector<uint8_t>& SaveDataBytes(size_t index);
}
