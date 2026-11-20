// Cemu's graphic packs, from the project (driver/cemu/graphicpacks.cpp).
#pragma once
#include <functional>
#include <string>
#include <vector>

namespace chimera
{
	// Puts the pack folders of one zip of the project's Graphic Packs slot
	// under folder, absolute in the machine's filesystem. False, and a text
	// key with its arguments in error, when it cannot be.
	bool MountGraphicPacks(const std::string& mounted, const std::string& name, const std::string& folder, std::string& error);

	// A choice a pack offers: one category of its [Preset] sections (a pack
	// with presets of no category has one, named ""), which Cemu's window
	// shows as a drop-down. The core declares each as a setting of the
	// project (docs/PLAN.md, "Choosing a pack's presets").
	struct PackChoice
	{
		std::string setting;             // the setting's name
		std::string display;             // what the settings grid calls it
		std::string folder;              // the pack's folder in the zip
		std::string category;            // upstream's category ("" for none)
		std::vector<std::string> presets; // every preset of it, in rules.txt's order
		std::string defaultPreset;       // the one upstream chooses unasked
	};

	// Loads every pack under Cemu's graphicPacks folder, as upstream does
	// when it starts, and turns each on. chosen(setting, value) gives the
	// preset the project chose for a choice (true), or says it chose none
	// (false: the pack's default stays). False, and a text key with its
	// arguments in error, when a project names a preset the pack lacks.
	// How many packs were loaded is in count.
	bool LoadGraphicPacks(const std::function<bool(const std::string&, std::string&)>& chosen, int& count, std::string& error);

	// The choices of the packs LoadGraphicPacks loaded, with the defaults
	// upstream gave them before the project chose.
	const std::vector<PackChoice>& GraphicPackChoices();

	// The choices as the JSON array of setting declarations a core answers
	// GetGameSettings and SuggestSettings with (Chimera's docs/porting-a-core.md).
	std::string PackSettingsJson();
}
