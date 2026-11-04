// Cemu's command line, which a core does not have: every setting keeps its
// default, and the driver does what the command line would.
#include "config/LaunchSettings.h"

std::optional<int> LaunchSettings::HandleCommandline(const wchar_t* lpCmdLine) { return {}; }
std::optional<int> LaunchSettings::HandleCommandline(int argc, wchar_t* argv[]) { return {}; }
std::optional<int> LaunchSettings::HandleCommandline(int argc, char* argv[]) { return {}; }
std::optional<int> LaunchSettings::HandleCommandline(const std::vector<std::wstring>& args) { return {}; }
bool LaunchSettings::ExtractorTool(std::wstring_view wud_path, std::string_view output_path, std::wstring_view log_path) { return false; }
