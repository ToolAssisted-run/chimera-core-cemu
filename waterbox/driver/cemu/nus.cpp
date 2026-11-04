// A game in the NUS form - a title's encrypted .app files with its
// title.tmd, title.tik and title.cert, the form the console downloads -
// given as a zip of that folder (the owner's decision of 2026-11-03). The
// zip is never read whole: its central directory says where each entry is,
// an entry stored uncompressed is read straight from the zip (a range of
// it grafted into the machine's filesystem), and a small compressed one is
// unpacked into the machine. A large compressed entry is refused: its
// bytes are encrypted, so compressing them gains nothing, and unpacking a
// gigabyte would make it part of every state.
// SPDX-License-Identifier: MIT
#include "chimera_nus.h"
#include "memfs.h"

#include <zlib.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <vector>

#define TEXT_ARG "\x1f"

namespace
{
	constexpr uint64_t kUnpackMost = 16u << 20; // a compressed entry unpacked into the machine, at most

	uint16_t le16(const uint8_t* p) { return (uint16_t)(p[0] | p[1] << 8); }
	uint32_t le32(const uint8_t* p) { return p[0] | p[1] << 8 | p[2] << 16 | uint32_t(p[3]) << 24; }
	uint64_t le64(const uint8_t* p) { return le32(p) | uint64_t(le32(p + 4)) << 32; }

	bool ReadAt(int fd, uint64_t at, void* out, size_t size)
	{
		uint8_t* p = static_cast<uint8_t*>(out);
		while (size)
		{
			const ssize_t got = pread(fd, p, size, (off_t)at);
			if (got <= 0)
				return false;
			p += got;
			at += (uint64_t)got;
			size -= (size_t)got;
		}
		return true;
	}

	struct Entry
	{
		std::string name;
		uint16_t method = 0, flags = 0;
		uint64_t packed = 0, size = 0, local = 0;
	};

	std::string Lower(std::string s)
	{
		std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
		return s;
	}

	// The zip's entries, from its central directory; false when it is no zip
	// or a damaged one
	bool Directory(int fd, uint64_t fileSize, std::vector<Entry>& entries)
	{
		// the end of the central directory: the last 22 bytes and a comment
		const uint64_t tail = std::min<uint64_t>(fileSize, 22 + 65535);
		if (tail < 22)
			return false;
		std::vector<uint8_t> t(tail);
		if (!ReadAt(fd, fileSize - tail, t.data(), t.size()))
			return false;
		size_t end = std::string::npos;
		for (size_t i = t.size() - 22 + 1; i-- > 0;)
		{
			if (le32(&t[i]) == 0x06054b50u)
			{
				end = i;
				break;
			}
		}
		if (end == std::string::npos)
			return false;
		uint64_t count = le16(&t[end + 10]);
		uint64_t cdSize = le32(&t[end + 12]);
		uint64_t cdAt = le32(&t[end + 16]);
		if (count == 0xFFFF || cdSize == 0xFFFFFFFFu || cdAt == 0xFFFFFFFFu)
		{
			// ZIP64: its locator just before, pointing at its own end record
			if (end < 20 || le32(&t[end - 20]) != 0x07064b50u)
				return false;
			const uint64_t z64 = le64(&t[end - 20 + 8]);
			uint8_t r[56];
			if (!ReadAt(fd, z64, r, sizeof r) || le32(r) != 0x06064b50u)
				return false;
			count = le64(r + 32);
			cdSize = le64(r + 40);
			cdAt = le64(r + 48);
		}
		if (cdAt > fileSize || cdSize > fileSize - cdAt || cdSize > (64u << 20))
			return false;
		std::vector<uint8_t> cd(cdSize);
		if (!ReadAt(fd, cdAt, cd.data(), cd.size()))
			return false;
		size_t at = 0;
		for (uint64_t k = 0; k < count; k++)
		{
			if (at + 46 > cd.size() || le32(&cd[at]) != 0x02014b50u)
				return false;
			Entry e;
			e.flags = le16(&cd[at + 8]);
			e.method = le16(&cd[at + 10]);
			e.packed = le32(&cd[at + 20]);
			e.size = le32(&cd[at + 24]);
			const uint16_t nameLength = le16(&cd[at + 28]), extra = le16(&cd[at + 30]), comment = le16(&cd[at + 32]);
			e.local = le32(&cd[at + 42]);
			if (at + 46 + nameLength + extra + comment > cd.size())
				return false;
			e.name.assign(reinterpret_cast<const char*>(&cd[at + 46]), nameLength);
			// ZIP64's extra field holds, in this order, the sizes and offset
			// that did not fit
			for (size_t x = at + 46 + nameLength; x + 4 <= at + 46 + nameLength + extra;)
			{
				const uint16_t id = le16(&cd[x]), length = le16(&cd[x + 2]);
				if (id == 0x0001)
				{
					size_t f = x + 4;
					const size_t fEnd = x + 4 + length;
					if (e.size == 0xFFFFFFFFu && f + 8 <= fEnd) { e.size = le64(&cd[f]); f += 8; }
					if (e.packed == 0xFFFFFFFFu && f + 8 <= fEnd) { e.packed = le64(&cd[f]); f += 8; }
					if (e.local == 0xFFFFFFFFu && f + 8 <= fEnd) { e.local = le64(&cd[f]); f += 8; }
				}
				x += 4 + length;
			}
			entries.push_back(e);
			at += 46 + nameLength + extra + comment;
		}
		return true;
	}
}

namespace chimera
{
	bool MountNusZip(const std::string& mounted, const std::string& name, const std::string& folder, std::string& tmdPath, std::string& error)
	{
		const int fd = open(mounted.c_str(), O_RDONLY);
		if (fd < 0)
		{
			error = "error.cannot_open" TEXT_ARG + name;
			return false;
		}
		struct stat st;
		std::vector<Entry> entries;
		const bool read = fstat(fd, &st) == 0 && Directory(fd, (uint64_t)st.st_size, entries);
		if (!read)
		{
			close(fd);
			error = "error.nus_not_zip" TEXT_ARG + name;
			return false;
		}
		// the folder: where title.tmd is (the zip's root, or one folder in it)
		std::string prefix;
		bool found = false;
		for (const Entry& e : entries)
		{
			const std::string lower = Lower(e.name);
			if (lower == "title.tmd" || (lower.size() > 10 && lower.compare(lower.size() - 10, 10, "/title.tmd") == 0 &&
				lower.find('/') == lower.size() - 10))
			{
				prefix = e.name.substr(0, e.name.size() - 9);
				found = true;
				break;
			}
		}
		if (!found)
		{
			close(fd);
			error = "error.nus_no_tmd" TEXT_ARG + name;
			return false;
		}
		memfs::mkdirs(folder);
		for (const Entry& e : entries)
		{
			if (e.name.size() <= prefix.size() || e.name.compare(0, prefix.size(), prefix) != 0)
				continue;
			const std::string leaf = e.name.substr(prefix.size());
			if (leaf.find('/') != std::string::npos)
				continue; // a folder, or a file in one: not the title's
			if (e.flags & 1)
			{
				close(fd);
				error = "error.nus_entry_locked" TEXT_ARG + name + TEXT_ARG + e.name;
				return false;
			}
			uint8_t lh[30];
			if (!ReadAt(fd, e.local, lh, sizeof lh) || le32(lh) != 0x04034b50u)
			{
				close(fd);
				error = "error.nus_damaged" TEXT_ARG + name;
				return false;
			}
			const uint64_t data = e.local + 30 + le16(lh + 26) + le16(lh + 28);
			if (data > (uint64_t)st.st_size || e.packed > (uint64_t)st.st_size - data)
			{
				close(fd);
				error = "error.nus_damaged" TEXT_ARG + name;
				return false;
			}
			const std::string path = folder + "/" + Lower(leaf);
			if (e.method == 0 && e.packed == e.size)
			{
				if (!memfs::graft_range(path, mounted, data, e.size))
				{
					close(fd);
					error = "error.cannot_open" TEXT_ARG + name;
					return false;
				}
			}
			else if (e.method == 8 && e.size <= kUnpackMost)
			{
				std::vector<uint8_t> packed(e.packed), bytes(e.size);
				z_stream z{};
				bool ok = ReadAt(fd, data, packed.data(), packed.size()) && inflateInit2(&z, -15) == Z_OK;
				if (ok)
				{
					z.next_in = packed.data();
					z.avail_in = (uInt)packed.size();
					z.next_out = bytes.data();
					z.avail_out = (uInt)bytes.size();
					const int r = inflate(&z, Z_FINISH);
					ok = r == Z_STREAM_END && z.total_out == e.size;
					inflateEnd(&z);
				}
				if (!ok || !memfs::put(path, std::move(bytes)))
				{
					close(fd);
					error = "error.nus_damaged" TEXT_ARG + name;
					return false;
				}
			}
			else
			{
				close(fd);
				error = "error.nus_entry_compressed" TEXT_ARG + name + TEXT_ARG + e.name;
				return false;
			}
		}
		close(fd);
		tmdPath = folder + "/title.tmd";
		return true;
	}
}
