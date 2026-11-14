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

	int LoadGraphicPacks()
	{
		// upstream's own search of its graphicPacks folder: every folder with
		// a rules.txt, in name order (the machine's filesystem lists so), is
		// a pack; one that is outdated or damaged is left out, as upstream
		// leaves it out (its log says why)
		GraphicPack2::LoadAll();
		// Upstream turns on the packs its settings name (settings.xml), with
		// the presets chosen there; a project's packs are all on, and no
		// preset is chosen, so each pack keeps the ones its rules.txt makes
		// the default.
		for (const auto& pack : GraphicPack2::GetGraphicPacks())
			pack->SetEnabled(true);
		return (int)GraphicPack2::GetGraphicPacks().size();
	}
}
