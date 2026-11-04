// The console's TV sound, as the frontend takes it: what the console's sound
// library hands its TV output during a frame, in the order it hands it.
// Upstream gives it to a host sound device; here it is kept for the frame.
#include "chimera_input.h"

#include "audio/IAudioAPI.h"

namespace
{
	// the frame's samples, pairs of 16-bit values at 48 kHz
	std::vector<int16_t> s_frame;

	class ChimeraAudio : public IAudioAPI
	{
	public:
		ChimeraAudio(uint32 samplerate, uint32 channels, uint32 samplesPerBlock)
			: IAudioAPI(samplerate, channels, samplesPerBlock, 16) {}

		AudioAPI GetType() const override { return Cubeb; } // asked by nobody here
		bool NeedAdditionalBlocks() const override { return false; }
		bool Play() override { m_playing = true; return true; }
		bool Stop() override { m_playing = false; return true; }

		// A block: m_samplesPerBlock samples of m_channels each. The TV is
		// stereo (the console's TV mode follows the setting Cemu reads, which
		// is stereo by default); any other count is folded to stereo.
		bool FeedBlock(sint16* data) override
		{
			for (uint32 i = 0; i < m_samplesPerBlock; i++)
			{
				const sint16* s = data + (size_t)i * m_channels;
				s_frame.push_back(s[0]);
				s_frame.push_back(m_channels > 1 ? s[1] : s[0]);
			}
			return true;
		}
	};
}

AudioAPIPtr chimera_make_tv_audio(sint32 rate, sint32 channels, sint32 samplesPerBlock)
{
	s_frame.reserve(16384);
	return std::make_unique<ChimeraAudio>(rate, channels, samplesPerBlock);
}

namespace chimera
{
	const int16_t* FrameAudio(int& pairs)
	{
		pairs = (int)(s_frame.size() / 2);
		return s_frame.data();
	}

	void BeginFrameAudio()
	{
		s_frame.clear();
	}
}
