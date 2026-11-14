// Cemu's graphic packs, from the project (driver/cemu/graphicpacks.cpp).
#pragma once
#include <string>

namespace chimera
{
	// Puts the pack folders of one zip of the project's Graphic Packs slot
	// under folder, absolute in the machine's filesystem. False, and a text
	// key with its arguments in error, when it cannot be.
	bool MountGraphicPacks(const std::string& mounted, const std::string& name, const std::string& folder, std::string& error);

	// Loads every pack under Cemu's graphicPacks folder, as upstream does
	// when it starts, and turns each on. How many packs were loaded.
	int LoadGraphicPacks();
}
