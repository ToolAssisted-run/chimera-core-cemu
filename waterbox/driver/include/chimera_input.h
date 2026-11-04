// The machine's controls, sound and screens as the frontend sees them: the
// Wii U GamePad's controls of the frame, the sound the console sent to the
// TV during it, and which of the two screens the picture shows.
#pragma once
#include <cstdint>

namespace chimera
{
	// The GamePad's buttons, in the package's order (waterbox.config): bit i
	// of FrameAdvance's mask is button i.
	enum PadButton : int
	{
		kPadUp, kPadDown, kPadLeft, kPadRight,
		kPadA, kPadB, kPadX, kPadY,
		kPadL, kPadR, kPadZL, kPadZR,
		kPadPlus, kPadMinus, kPadHome,
		kPadStickL, kPadStickR,
		kPadTouch,
		kPadButtonCount
	};
	// Its axes, in the package's order: the sticks -32768..32767 with up and
	// right positive; the touch point 0..65535, a fraction of the whole
	// picture the core hands out.
	enum PadAxis : int
	{
		kAxisLeftX, kAxisLeftY, kAxisRightX, kAxisRightY,
		kAxisTouchX, kAxisTouchY,
		kPadAxisCount
	};

	// the controls held during the next frame
	void SetPadButtons(uint64_t mask);
	void SetPadAxis(int index, int32_t value);
	// Did the console read the GamePad during the last frame? (A frame in
	// which it did not is a lag frame.)
	bool InputWasRead();
	void BeginFrameInput();

	// What the picture shows: the TV above the GamePad, the TV alone, or the
	// GamePad alone. A setting, read at Init.
	enum class Screens { Both, TV, GamePad };
	void SetScreens(Screens screens);
	Screens GetScreens();
	bool ShowsGamePad();
	constexpr int kPadWidth = 854, kPadHeight = 480;
	// the picture's size, and where the GamePad's screen is in it
	void PictureSize(int& w, int& h);
	void GamePadArea(int& x, int& y);

	// The sound of the last frame: pairs of 16-bit samples at 48 kHz.
	const int16_t* FrameAudio(int& pairs);
	void BeginFrameAudio();
}

// called where the console reads its controls (wbx-entry.cpp: the frontend's
// input poll callback, when it gave one)
void chimera_input_polled();
