// Cemu's graphic packs, from the project's Graphic Packs slot (chimera issue
// 585; docs/PLAN.md, "Graphic packs"). Each file of the slot is a zip of one
// or more pack folders, each with its rules.txt. Its entries are put where
// upstream Cemu keeps the packs it has (graphicPacks in its data folder),
// one folder a zip, in the slot's order: an entry stored uncompressed is
// read straight from the zip (a range of it grafted into the machine's
// filesystem, so no state carries it), a compressed one is unpacked into
// the machine. Then Cemu loads them as its own window does, and every pack
// is turned on; at power-on Cemu activates those that name the title it
// starts, each with the presets upstream chooses when the user has chosen
// none. Nothing is downloaded: there is no list of the community's packs.
// SPDX-License-Identifier: MIT
#include "chimera_graphicpacks.h"
#include "chimera_zip.h"
#include "memfs.h"

#include "Cafe/GraphicPack/GraphicPack2.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <vector>

#define TEXT_ARG "\x1f"

namespace
{
	// a compressed entry unpacked into the machine, at most: what is
	// unpacked is in every state
	constexpr uint64_t kUnpackMost = 64u << 20;

	// a name the zip may hold: relative, with no part that leaves the folder
	bool SafeName(const std::string& name)
	{
		if (name.empty() || name[0] == '/' || name.find('\\') != std::string::npos || name.find(':') != std::string::npos)
			return false;
		size_t at = 0;
		while (at <= name.size())
		{
			size_t end = name.find('/', at);
			if (end == std::string::npos)
				end = name.size();
			const std::string part = name.substr(at, end - at);
			if (part == "..")
				return false;
			at = end + 1;
		}
		return true;
	}

	bool IsRules(const std::string& name)
	{
		return name == "rules.txt" || (name.size() > 10 && name.compare(name.size() - 10, 10, "/rules.txt") == 0);
	}
}

namespace chimera
{
	bool MountGraphicPacks(const std::string& mounted, const std::string& name, const std::string& folder, std::string& error)
	{
		using namespace zip;
		const int fd = open(mounted.c_str(), O_RDONLY);
		if (fd < 0)
		{
			error = "error.cannot_open" TEXT_ARG + name;
			return false;
		}
		struct stat st;
		std::vector<Entry> entries;
		if (fstat(fd, &st) != 0 || !Directory(fd, (uint64_t)st.st_size, entries))
		{
			close(fd);
			error = "error.packs_not_zip" TEXT_ARG + name;
			return false;
		}
		const uint64_t fileSize = (uint64_t)st.st_size;
		bool rules = false;
		for (const Entry& e : entries)
			if (IsRules(e.name))
				rules = true;
		if (!rules)
		{
			close(fd);
			error = "error.packs_no_rules" TEXT_ARG + name;
			return false;
		}
		auto fail = [&](const char* key, const std::string& entry) {
			close(fd);
			error = std::string(key) + TEXT_ARG + name;
			if (!entry.empty())
				error += TEXT_ARG + entry;
			return false;
		};
		memfs::mkdirs(folder);
		for (const Entry& e : entries)
		{
			if (!SafeName(e.name))
				return fail("error.packs_damaged", "");
			const std::string path = folder + "/" + e.name;
			if (e.name.back() == '/')
			{
				memfs::mkdirs(path.substr(0, path.size() - 1));
				continue;
			}
			if (e.flags & 1)
				return fail("error.packs_entry_method", e.name);
			uint64_t data = 0;
			if (!DataAt(fd, fileSize, e, data))
				return fail("error.packs_damaged", "");
			memfs::mkdirs(path.substr(0, path.rfind('/')));
			if (e.method == 0 && e.packed == e.size)
			{
				if (!memfs::graft_range(path, mounted, data, e.size))
					return fail("error.cannot_open", "");
			}
			else if (e.method == 8)
			{
				if (e.size > kUnpackMost)
					return fail("error.packs_entry_too_big", e.name);
				std::vector<uint8_t> bytes;
				if (!Inflate(fd, data, e, bytes))
					return fail("error.packs_entry_not_unpacked", e.name);
				if (!memfs::put(path, std::move(bytes)))
					return fail("error.packs_entry_not_unpacked", e.name);
			}
			else
				return fail("error.packs_entry_method", e.name);
		}
		close(fd);
		return true;
	}

	namespace
	{
		std::vector<PackChoice> s_choices;

		// letters, digits and . - _ only; every other run of characters
		// becomes one underscore
		std::string Plain(const std::string& text)
		{
			std::string out;
			for (unsigned char ch : text)
			{
				if (isalnum(ch) || ch == '.' || ch == '-' || ch == '_')
					out += (char)ch;
				else if (out.empty() || out.back() != '_')
					out += '_';
			}
			return out;
		}
	}

	const std::vector<PackChoice>& GraphicPackChoices()
	{
		return s_choices;
	}

	namespace
	{
		std::string Json(const std::string& s)
		{
			std::string out = "\"";
			for (char ch : s)
			{
				if (ch == '"' || ch == '\\')
				{
					out += '\\';
					out += ch;
				}
				else if ((unsigned char)ch < 0x20)
				{
					char b[8];
					snprintf(b, sizeof b, "\\u%04x", ch);
					out += b;
				}
				else
					out += ch;
			}
			return out + "\"";
		}
	}

	std::string PackSettingsJson()
	{
		std::string out = "[";
		for (const PackChoice& c : s_choices)
		{
			out += out.size() > 1 ? "," : "";
			out += "{\"name\":" + Json(c.setting) + ",\"display\":" + Json(c.display) + ",\"type\":\"enum\",\"options\":[";
			for (size_t i = 0; i < c.presets.size(); i++)
				out += (i ? "," : "") + Json(c.presets[i]);
			out += "],\"default\":" + Json(c.defaultPreset);
			out += ",\"description\":" + Json("game_setting.pack" TEXT_ARG + c.folder) + "}";
		}
		return out + "]";
	}

	bool LoadGraphicPacks(const std::function<bool(const std::string&, std::string&)>& chosen, int& count, std::string& error)
	{
		s_choices.clear();
		// upstream's own search of its graphicPacks folder: every folder with
		// a rules.txt, in name order (the machine's filesystem lists so), is
		// a pack; one that is outdated or damaged is left out, as upstream
		// leaves it out (its log says why)
		GraphicPack2::LoadAll();
		count = (int)GraphicPack2::GetGraphicPacks().size();
		for (const auto& pack : GraphicPack2::GetGraphicPacks())
		{
			// the pack's folder as the zip has it: upstream's path, without
			// graphicPacks/ and the zip's place in the slot (000/), so that
			// a choice keeps its name when the slot's order changes
			std::string folder = pack->GetNormalizedPathString();
			if (folder.compare(0, 13, "graphicPacks/") == 0)
				folder.erase(0, 13);
			const size_t slash = folder.find('/');
			if (slash != std::string::npos)
				folder.erase(0, slash + 1);
			if (folder.size() > 10 && folder.compare(folder.size() - 10, 10, "/rules.txt") == 0)
				folder.erase(folder.size() - 10);
			std::vector<std::string> order;
			const auto categories = pack->GetCategorizedPresets(order);
			bool changed = false;
			for (const std::string& category : order)
			{
				const auto& presets = categories.at(category);
				if (presets.size() < 2)
					continue;
				PackChoice choice;
				choice.folder = folder;
				choice.category = category;
				for (const auto& preset : presets)
					if (std::find(choice.presets.begin(), choice.presets.end(), preset->name) == choice.presets.end())
						choice.presets.push_back(preset->name);
				if (choice.presets.size() < 2)
					continue;
				// what upstream chose when nothing was said: the preset marked
				// default = 1, else the first (the constructor and
				// ValidatePresetSelections made it active)
				choice.defaultPreset = pack->GetActivePreset(category);
				if (choice.defaultPreset.empty())
					choice.defaultPreset = choice.presets[0];
				std::string name = "pack." + Plain(folder) + (category.empty() ? "" : "." + Plain(category));
				for (int n = 2; std::any_of(s_choices.begin(), s_choices.end(), [&](const PackChoice& o) { return o.setting == name; }); n++)
					name = "pack." + Plain(folder) + "." + Plain(category) + "." + std::to_string(n);
				choice.setting = name;
				const std::string& label = pack->GetVirtualPath().empty() ? folder : pack->GetVirtualPath();
				choice.display = category.empty() ? label : label + " - " + category;
				std::string value;
				if (chosen(choice.setting, value) && value != choice.defaultPreset)
				{
					if (std::find(choice.presets.begin(), choice.presets.end(), value) == choice.presets.end())
					{
						error = "error.packs_preset" TEXT_ARG + choice.setting + TEXT_ARG + value;
						return false;
					}
					pack->SetActivePreset(category, value, false);
					changed = true;
				}
				s_choices.push_back(std::move(choice));
			}
			if (changed)
			{
				// a preset chosen can hide or show the presets of another
				// category (a pack of version 5 and later), as in Cemu's
				// window
				pack->UpdatePresetVisibility();
				pack->ValidatePresetSelections();
			}
		}
		// Upstream turns on the packs its settings name (settings.xml); a
		// project's packs are all on, with the presets the project chose or,
		// for a choice it did not make, the ones rules.txt makes the default.
		for (const auto& pack : GraphicPack2::GetGraphicPacks())
			pack->SetEnabled(true);
		return true;
	}
}
