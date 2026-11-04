// What Cemu asks of the host that a core answers the same way everywhere:
// no crash handler (the sandbox reports a dying guest itself), and a
// processor and memory that are the same on every machine.
#include "Common/ExceptionHandler/ExceptionHandler.h"
#include "util/SystemInfo/SystemInfo.h"

void ExceptionHandler_Init() {}
bool CrashLog_Create() { return false; }
void CrashLog_SetOutputChannels(bool writeToStdErr, bool writeToLogTxt) {}
void CrashLog_WriteLine(std::string_view text, bool newLine) {}
void CrashLog_WriteHeader(const char* header) {}
void ExceptionHandler_LogGeneralInfo() {}

uint64 QueryRamUsage() { return 0; }
void QueryProcTime(uint64& out_now, uint64& out_user, uint64& out_kernel) { out_now = out_user = out_kernel = 0; }
void QueryCoreTimes(uint32 count, std::vector<ProcessorTime>& out)
{
	for (auto& t : out)
		t = {};
}
