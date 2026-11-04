// The console's saved games, in and out (chimera docs/save-data.md). They
// live where the console keeps them, the storage's usr/save folder (in the
// machine's filesystem, so every state carries them). Out: every file under
// it, named from the storage's root ("usr/save/00050000/.../user/80000001/
// file"). In: a zip as Emulator > Export Save Data wrote it, every entry
// under usr/save/, written into the storage before the console starts.
// Stored and deflated entries, as zip tools write them (the reader is the
// Xenia core's).
// SPDX-License-Identifier: MIT
#include "chimera_savedata.h"
#include "memfs.h"

#include <zlib.h>

#include <cstdio>
#include <cstring>

#define TEXT_ARG "\x1f"

namespace
{
	struct SavedFile
	{
		std::string name;
		std::vector<uint8_t> data;
	};
	std::vector<SavedFile> s_saved;
	const std::string s_noName;
	const std::vector<uint8_t> s_noBytes;

	uint32_t le32(const uint8_t* p) { return p[0] | p[1] << 8 | p[2] << 16 | uint32_t(p[3]) << 24; }
	uint16_t le16(const uint8_t* p) { return (uint16_t)(p[0] | p[1] << 8); }
}

namespace chimera
{
	bool LoadSaveData(const std::string& mounted, const std::string& name, const std::string& storage, std::string& error)
	{
		std::vector<uint8_t> zip;
		if (FILE* f = std::fopen(mounted.c_str(), "rb"))
		{
			uint8_t buf[65536];
			size_t n;
			while ((n = std::fread(buf, 1, sizeof buf, f)) > 0)
				zip.insert(zip.end(), buf, buf + n);
			std::fclose(f);
		}
		// the end of the central directory: the last 22 bytes and a comment
		size_t end = std::string::npos;
		for (size_t i = zip.size() >= 22 ? zip.size() - 22 : 0; zip.size() >= 22; i--)
		{
			if (le32(&zip[i]) == 0x06054b50u)
			{
				end = i;
				break;
			}
			if (i == 0 || zip.size() - i > 22 + 65535)
				break;
		}
		if (end == std::string::npos)
		{
			error = std::string("error.save_not_zip" TEXT_ARG) + name;
			return false;
		}
		const uint16_t count = le16(&zip[end + 10]);
		size_t at = le32(&zip[end + 16]);
		for (uint16_t k = 0; k < count; k++)
		{
			if (at + 46 > zip.size() || le32(&zip[at]) != 0x02014b50u)
			{
				error = std::string("error.save_damaged" TEXT_ARG) + name;
				return false;
			}
			const uint16_t method = le16(&zip[at + 10]);
			const uint32_t packed = le32(&zip[at + 20]), size = le32(&zip[at + 24]);
			const uint16_t nameLength = le16(&zip[at + 28]), extra = le16(&zip[at + 30]), comment = le16(&zip[at + 32]);
			const uint32_t local = le32(&zip[at + 42]);
			const std::string entry(reinterpret_cast<const char*>(&zip[at + 46]), nameLength);
			at += 46 + nameLength + extra + comment;
			if (!entry.empty() && entry.back() == '/')
				continue;
			if (entry.rfind("usr/save/", 0) != 0 || entry.find("..") != std::string::npos)
			{
				error = std::string("error.save_not_storage" TEXT_ARG) + name + TEXT_ARG + entry;
				return false;
			}
			if (local + 30 > zip.size() || le32(&zip[local]) != 0x04034b50u)
			{
				error = std::string("error.save_damaged" TEXT_ARG) + name;
				return false;
			}
			const size_t data = local + 30 + le16(&zip[local + 26]) + le16(&zip[local + 28]);
			if (data + packed > zip.size())
			{
				error = std::string("error.save_damaged" TEXT_ARG) + name;
				return false;
			}
			std::vector<uint8_t> bytes(size);
			if (method == 0 && packed == size)
			{
				if (size)
					std::memcpy(bytes.data(), &zip[data], size);
			}
			else if (method == 8)
			{
				z_stream z{};
				if (inflateInit2(&z, -15) != Z_OK)
				{
					error = std::string("error.save_entry_not_unpacked" TEXT_ARG) + name + TEXT_ARG + entry;
					return false;
				}
				z.next_in = &zip[data];
				z.avail_in = packed;
				z.next_out = bytes.data();
				z.avail_out = size;
				const int r = inflate(&z, Z_FINISH);
				inflateEnd(&z);
				if (r != Z_STREAM_END || z.total_out != size)
				{
					error = std::string("error.save_entry_not_unpacked" TEXT_ARG) + name + TEXT_ARG + entry;
					return false;
				}
			}
			else
			{
				error = std::string("error.save_entry_method" TEXT_ARG) + name + TEXT_ARG + entry;
				return false;
			}
			const std::string path = storage + "/" + entry;
			if (!memfs::mkdirs(path.substr(0, path.rfind('/'))) || !memfs::put(path, std::move(bytes)))
			{
				error = std::string("error.save_entry_not_written" TEXT_ARG) + name + TEXT_ARG + entry;
				return false;
			}
		}
		return true;
	}

	size_t SaveDataSnapshot(const std::string& storage)
	{
		s_saved.clear();
		const std::string saves = storage + "/usr/save";
		for (const std::string& relative : memfs::list(saves))
		{
			SavedFile file;
			file.name = "usr/save/" + relative;
			if (memfs::get(saves + "/" + relative, file.data))
				s_saved.push_back(std::move(file));
		}
		return s_saved.size();
	}

	const std::string& SaveDataName(size_t index)
	{
		return index < s_saved.size() ? s_saved[index].name : s_noName;
	}

	const std::vector<uint8_t>& SaveDataBytes(size_t index)
	{
		return index < s_saved.size() ? s_saved[index].data : s_noBytes;
	}
}
