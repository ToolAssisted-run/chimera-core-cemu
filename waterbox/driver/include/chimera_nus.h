// A game in the NUS form, given as a zip of its folder (driver/cemu/nus.cpp).
#pragma once
#include <string>

namespace chimera
{
	// Mounts the title in the zip (its files at the zip's root or in one
	// folder of it) under folder, absolute in the machine's filesystem;
	// tmdPath is then its title.tmd there. False, and a text key with its
	// arguments in error, when it cannot be.
	bool MountNusZip(const std::string& mounted, const std::string& name, const std::string& folder, std::string& tmdPath, std::string& error);
}
