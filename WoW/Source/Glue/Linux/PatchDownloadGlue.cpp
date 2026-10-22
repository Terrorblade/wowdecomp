#if defined(WOWDECOMP_PLATFORM_POSIX)

#include <cstdint>
#include "WoW/Source/Glue/CGlueMgr.h"

// The downloaded patcher is a Windows executable that cannot run natively, so launching it fails, as a failed
// ShellExecute/CreateProcess does on Windows.
bool CGlueMgr::PatchDownloadExecuteCommand(const char* path, const char* arguments) {
    return false;
}

#endif
