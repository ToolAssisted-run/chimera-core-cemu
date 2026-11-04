// The window Cemu would draw in, for a core that has none. The picture is
// read back by the driver; nothing here is shown to anybody.
#include "WindowSystem.h"
#include "chimera_input.h"

namespace WindowSystem
{
	static WindowInfo s_windowInfo;

	void ShowErrorDialog(std::string_view message, std::string_view title, std::optional<ErrorCategory> errorCategory)
	{
		cemuLog_log(LogType::Force, "error dialog: {} {}", title, message);
	}

	void Create()
	{
		s_windowInfo.width = 1280;
		s_windowInfo.height = 720;
		s_windowInfo.phys_width = 1280;
		s_windowInfo.phys_height = 720;
		s_windowInfo.dpi_scale = 1.0;
		s_windowInfo.pad_open = chimera::ShowsGamePad();
		s_windowInfo.app_active = true;
	}

	WindowInfo& GetWindowInfo() { return s_windowInfo; }
	void UpdateWindowTitles(bool isIdle, bool isLoading, double fps) {}
	void GetWindowSize(int& w, int& h) { w = 1280; h = 720; }
	void GetPadWindowSize(int& w, int& h) { w = 854; h = 480; }
	void GetWindowPhysSize(int& w, int& h) { w = 1280; h = 720; }
	void GetPadWindowPhysSize(int& w, int& h) { w = 854; h = 480; }
	double GetWindowDPIScale() { return 1.0; }
	double GetPadDPIScale() { return 1.0; }
	bool IsPadWindowOpen() { return chimera::ShowsGamePad(); } // the GamePad's screen is in the picture
	bool IsKeyDown(uint32 key) { return false; }
	bool IsKeyDown(PlatformKeyCodes key) { return false; }
	std::string GetKeyCodeName(uint32 key) { return {}; }
	bool InputConfigWindowHasFocus() { return false; }
	void NotifyGameLoaded() {}
	void NotifyGameExited() {}
	void RefreshGameList() {}
	bool IsFullScreen() { return false; }
	void CaptureInput(const ControllerState& currentState, const ControllerState& lastState) {}
}
