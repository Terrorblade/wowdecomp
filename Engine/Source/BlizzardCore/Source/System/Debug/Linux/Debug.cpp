#if defined(WOWDECOMP_PLATFORM_POSIX)

#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "Engine/Source/BlizzardCore/Source/System/Debug/Debug.h"
#include "Engine/Source/BlizzardCore/Source/System/File/File.h"
#include "Engine/Source/BlizzardCore/Source/System/File/Linux/LinuxPath.h"
#include "Engine/Source/BlizzardCore/Source/System/Lock/Lock.h"
#include "Engine/Source/BlizzardCore/Source/System/Process/Process.h"
#include "Engine/Source/BlizzardCore/Source/System/String/Path.h"
#include "Engine/Source/BlizzardCore/Source/System/String/String.h"
#include "Engine/Source/BlizzardCore/Source/System/Time/Time.h"
#include "Engine/Source/Os/Linux/OsPosix.h"

static bool s_continueAfterAssert;

static bool DebuggerAttached() {
    return OsPosixDebuggerAttached();
}

void Blizzard::Debug::Breakpoint() {
    // Windows swallows the breakpoint exception when no debugger is attached; SIGTRAP would kill the process instead.
    if (DebuggerAttached()) {
        raise(SIGTRAP);
    }
}

void Blizzard::Debug::DefaultCallback(const char* expression, const char* message, const char* fileName, uint32_t lineNumber) {
    char text[2048];
    char path[512];
    char canonical[256];
    char directory[512];
    Blizzard::Time::TimeRec rec;

    Blizzard::Time::BreakTime(Blizzard::Time::GetTimestamp(), rec);
    Blizzard::String::Format(text, sizeof(text), "%04d/%02d/%02d %02d:%02d:%02d  %s(%d) : Assertion failed: %s  \"%s\"\n", rec.year, rec.month, rec.day, rec.hour, rec.minute, rec.second, CanonicalStr(fileName, canonical, sizeof(canonical)), lineNumber, expression, message);
    fprintf(stderr, "%s", text);

    Blizzard::Process::GetProcessDirectory(directory, sizeof(directory));
    Blizzard::String::Append(directory, "Errors", sizeof(directory));
    Blizzard::File::CreateDirectory(directory, true);
    Blizzard::String::Append(directory, "/Assert.log", sizeof(directory));

    FILE* stream = nullptr;

    if (System_File::LinuxResolvePath(directory, path, sizeof(path), true)) {
        stream = fopen(path, "a");
    }

    if (stream) {
        fprintf(stream, "%s", text);
        fclose(stream);
    }

    Breakpoint();

    if (!s_continueAfterAssert) {
        abort();
    }
}

void System_Debug::ShutdownProc() {
    // Windows unloads dbghelp.dll here; nothing is loaded for stack walking on Linux.
}

#endif
