// The Wii U GamePad, as the console reads it (VPADRead): its controls are the
// frame's input (chimera_input.h). Upstream reads a host controller through
// its input manager, with dead zones, a host clock for the button repeat and
// the mouse for the touch screen; here every value reaches the console as the
// frame gave it, and the repeat counts the console's time.
//
// Everything here is the machine's: it is in the console's process, so a
// state carries it.
#include "chimera_input.h"

#include "Cafe/OS/libs/vpad/vpad.h"
#include "Cafe/OS/libs/coreinit/coreinit_Time.h"

namespace
{
	using namespace chimera;

	uint64_t s_buttons = 0;
	int32_t s_axes[kPadAxisCount] = {0, 0, 0, 0, 32768, 32768};
	bool s_read = false;
	Screens s_screens = Screens::Both;

	// what the console was told last: VPAD's trig and release are the change
	// from it, and the repeat counts from when it changed
	uint32 s_lastHold = 0;
	uint64 s_lastHoldChangeMs = 0, s_lastPulseMs = 0;
	// where the screen was last touched: a game may read it after the touch
	// ended (upstream keeps it for NGDK games)
	uint16 s_lastTouchX = 0, s_lastTouchY = 0;

	// the console's buttons (VPAD_BUTTON_*)
	enum : uint32
	{
		kVpadA = 0x8000, kVpadB = 0x4000, kVpadX = 0x2000, kVpadY = 0x1000,
		kVpadL = 0x0020, kVpadR = 0x0010, kVpadZL = 0x0080, kVpadZR = 0x0040,
		kVpadPlus = 0x0008, kVpadMinus = 0x0004, kVpadHome = 0x0002,
		kVpadUp = 0x0200, kVpadDown = 0x0100, kVpadLeft = 0x0800, kVpadRight = 0x0400,
		kVpadStickR = 0x00020000, kVpadStickL = 0x00040000,
		kVpadStickLUp = 0x10000000, kVpadStickLDown = 0x08000000, kVpadStickLLeft = 0x40000000, kVpadStickLRight = 0x20000000,
		kVpadStickRUp = 0x01000000, kVpadStickRDown = 0x00800000, kVpadStickRLeft = 0x04000000, kVpadStickRRight = 0x02000000,
		kVpadRepeat = 0x80000000,
	};

	constexpr uint32 kButtonFlags[kPadButtonCount] = {
		kVpadUp, kVpadDown, kVpadLeft, kVpadRight,
		kVpadA, kVpadB, kVpadX, kVpadY,
		kVpadL, kVpadR, kVpadZL, kVpadZR,
		kVpadPlus, kVpadMinus, kVpadHome,
		kVpadStickL, kVpadStickR,
		0, // Touch: the touch screen, not a button
	};

	bool Held(int button)
	{
		return (s_buttons >> button) & 1;
	}

	float Stick(int axis)
	{
		const float v = (float)s_axes[axis] / 32767.0f;
		return v < -1.0f ? -1.0f : (v > 1.0f ? 1.0f : v);
	}

	// a stick past half way is also a direction (upstream's thresholds: half
	// way on, a tenth to stay on)
	uint32 StickDirections(float x, float y, uint32 lastHold, uint32 left, uint32 right, uint32 down, uint32 up)
	{
		constexpr float kOn = 0.5f, kStay = 0.1f;
		uint32 hold = 0;
		if (x <= -kOn || ((lastHold & left) && x <= -kStay))
			hold |= left;
		else if (x >= kOn || ((lastHold & right) && x >= kStay))
			hold |= right;
		if (y <= -kOn || ((lastHold & down) && y <= -kStay))
			hold |= down;
		else if (y >= kOn || ((lastHold & up) && y >= kStay))
			hold |= up;
		return hold;
	}

	uint64 ConsoleMs()
	{
		return coreinit::OSGetTime() / (ESPRESSO_TIMER_CLOCK / 1000);
	}
}

namespace chimera
{
	void SetPadButtons(uint64_t mask)
	{
		s_buttons = mask;
	}

	void SetPadAxis(int index, int32_t value)
	{
		if (index >= 0 && index < kPadAxisCount)
			s_axes[index] = value;
	}

	bool InputWasRead()
	{
		return s_read;
	}

	void BeginFrameInput()
	{
		s_read = false;
	}

	void SetScreens(Screens screens)
	{
		s_screens = screens;
	}

	Screens GetScreens()
	{
		return s_screens;
	}

	bool ShowsGamePad()
	{
		return s_screens != Screens::TV;
	}

	void PictureSize(int& w, int& h)
	{
		switch (s_screens)
		{
		case Screens::TV: w = 1280; h = 720; break;
		case Screens::GamePad: w = kPadWidth; h = kPadHeight; break;
		default: w = 1280; h = 720 + kPadHeight; break;
		}
	}

	void GamePadArea(int& x, int& y)
	{
		if (s_screens == Screens::GamePad)
		{
			x = 0;
			y = 0;
		}
		else
		{
			x = (1280 - kPadWidth) / 2;
			y = 720;
		}
	}
}

bool chimera_gamepad_connected(sint32 channel)
{
	return channel == 0;
}

// VPADRead's sample of the GamePad: upstream's VPADController::VPADRead, on
// the frame's controls and the console's clock.
void chimera_gamepad_read(VPADStatus_t& status, const BtnRepeat& repeat)
{
	s_read = true;
	chimera_input_polled();

	uint32 hold = 0;
	for (int i = 0; i < kPadButtonCount; i++)
		if (Held(i))
			hold |= kButtonFlags[i];

	const float lx = Stick(kAxisLeftX), ly = Stick(kAxisLeftY);
	const float rx = Stick(kAxisRightX), ry = Stick(kAxisRightY);
	status.leftStick.x = lx;
	status.leftStick.y = ly;
	status.rightStick.x = rx;
	status.rightStick.y = ry;
	hold |= StickDirections(lx, ly, s_lastHold, kVpadStickLLeft, kVpadStickLRight, kVpadStickLDown, kVpadStickLUp);
	hold |= StickDirections(rx, ry, s_lastHold, kVpadStickRLeft, kVpadStickRRight, kVpadStickRDown, kVpadStickRUp);
	status.hold = hold;

	// the repeat, in upstream's units (milliseconds) of the console's time
	const uint64 now = ConsoleMs();
	const uint32 currentHold = hold;
	if (currentHold != s_lastHold)
		s_lastHoldChangeMs = s_lastPulseMs = now;
	if (repeat.pulse > 0 && currentHold != 0 && s_lastHoldChangeMs + (uint64)repeat.delay <= now &&
		s_lastPulseMs + (uint64)repeat.pulse < now)
	{
		s_lastPulseMs = now;
		status.hold = hold | kVpadRepeat;
	}
	status.release = s_lastHold & ~currentHold;
	status.trig = ~s_lastHold & currentHold;
	s_lastHold = currentHold;

	// the touch screen: the point is a fraction of the whole picture; it
	// touches only where the picture shows the GamePad's screen
	status.tpData.touch = kTpTouchOff;
	status.tpData.validity = kTpInvalid;
	status.tpData.x = s_lastTouchX;
	status.tpData.y = s_lastTouchY;
	if (Held(kPadTouch) && ShowsGamePad())
	{
		int w, h, x0, y0;
		chimera::PictureSize(w, h);
		chimera::GamePadArea(x0, y0);
		const double px = (double)s_axes[kAxisTouchX] * w / 65536.0 - x0;
		const double py = (double)s_axes[kAxisTouchY] * h / 65536.0 - y0;
		if (px >= 0 && py >= 0 && px < kPadWidth && py < kPadHeight)
		{
			const float fx = (float)(px / kPadWidth), fy = (float)(py / kPadHeight);
			status.tpData.touch = kTpTouchOn;
			status.tpData.validity = kTpValid;
			status.tpData.x = s_lastTouchX = (uint16)(fx * 3883.0f + 92.0f);
			status.tpData.y = s_lastTouchY = (uint16)(4095.0f - fy * 3694.0f - 254.0f);
		}
	}
	status.tpProcessed1 = status.tpData;
	status.tpProcessed2 = status.tpData;

	// held still, face up (upstream's answer without a motion sensor)
	status.dir.x = {1, 0, 0};
	status.dir.y = {0, 1, 0};
	status.dir.z = {0, 0, 1};
	status.accXY = {1.0f, 0.0f};
}
