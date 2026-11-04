// Cemu's per-game profiles, as upstream ships them (bin/gameProfiles/default,
// one .ini a title id): a few titles run with settings of their own, such as
// Super Mario 3D World with the GPU's multiplication done the faster way
// (accurateShaderMul), or Minecraft with the console's libraries rather than
// its own copies (loadSharedLibraries). They are compiled into the core
// (waterbox/CMakeLists.txt makes game_profiles.inc) and written into the
// machine's data folder before Cemu reads the profile of the title it runs,
// so a title gets the same profile on every host and with every build of
// the same core.
#include "chimera_profiles.h"
#include "memfs.h"

#include <cstdint>
#include <vector>

namespace
{
	struct Profile
	{
		const char* name;
		std::vector<uint8_t> bytes;
	};

	const Profile kProfiles[] = {
#include "game_profiles.inc"
	};
}

namespace chimera
{
	int WriteGameProfiles(const std::string& folder)
	{
		if (!memfs::mkdirs(folder))
			return -1;
		int written = 0;
		for (const Profile& profile : kProfiles)
		{
			if (!memfs::put(folder + "/" + profile.name, profile.bytes))
				return -1;
			written++;
		}
		return written;
	}
}
