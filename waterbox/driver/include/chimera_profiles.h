#pragma once
// Cemu's per-game profiles, compiled into the core (driver/cemu/profiles.cpp)
#include <string>

namespace chimera
{
	// writes every profile into the folder of the machine's filesystem;
	// how many, or -1 when the filesystem refused one
	int WriteGameProfiles(const std::string& folder);
}
