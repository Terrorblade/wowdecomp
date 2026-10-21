#if defined(WOWDECOMP_PLATFORM_LINUX)

#include "Engine/Source/Os/System.h"
#include "Engine/Source/Storm/H/storm.h"
#include "WoW/Source/Client.h"

// The Windows client runs CommonMain on a fiber with an 8 MB stack (WinMain); the main thread's stack on Linux is
// already 8 MB by default, so main calls it directly. The MSVC floating-point setup in the Windows main (all
// exceptions masked, round to nearest, denormals kept) is the default MXCSR state here.
int main(int argc, char** argv) {
    OsSetCommandLine(argc, argv);
    StormRtlInitialize();

    return CommonMain();
}

#endif
