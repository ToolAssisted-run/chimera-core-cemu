// The core's zip reader (driver/cemu/nus.cpp): a game's NUS zip and the
// project's graphic packs are read with it. It reads the central directory,
// ZIP64 included, and never the whole zip.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace chimera::zip
{
	struct Entry
	{
		std::string name;
		uint16_t method = 0, flags = 0;
		uint64_t packed = 0, size = 0, local = 0;
	};

	uint16_t le16(const uint8_t* p);
	uint32_t le32(const uint8_t* p);

	// size bytes at the offset at of the open file fd; false when short
	bool ReadAt(int fd, uint64_t at, void* out, size_t size);

	// the zip's entries, from its central directory; false when it is no zip
	// or a damaged one
	bool Directory(int fd, uint64_t fileSize, std::vector<Entry>& entries);

	// where an entry's bytes begin in the zip, from its local header; false
	// when the zip is damaged there
	bool DataAt(int fd, uint64_t fileSize, const Entry& entry, uint64_t& data);

	// a deflated entry's bytes, unpacked; false when they cannot be
	bool Inflate(int fd, uint64_t data, const Entry& entry, std::vector<uint8_t>& out);
}
