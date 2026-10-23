
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <set>
#include <string>
#include <windows.h>
#include "Engine/Source/BlizzardCore/Source/System/File/File.h"
#include "Engine/Source/BlizzardCore/Source/System/Memory/TempBuffer.h"
#include "Engine/Source/BlizzardCore/Source/System/Process/Process.h"
#include "Engine/Source/BlizzardCore/Source/System/String/Path.h"
#include "Engine/Source/BlizzardCore/Source/System/String/String.h"
#include "Engine/Source/BlizzardCore/Source/System/Unicode/Unicode.h"

struct ChildProcess {
    /* 0000 */ HANDLE m_process;
    /* 0004 */ void (*m_callback)(void*);
    /* 0008 */ void* m_param;
};

// STATIC: 0x00B33FF4
static UINT_PTR s_timer;

// STATIC: 0x00B38310
static std::set<ChildProcess*> s_childProcesses;

// OFFSET: 0x0043DEC0
static void CALLBACK TimerFunc(HWND hwnd, UINT message, UINT_PTR id, DWORD time) {
    for (std::set<ChildProcess*>::iterator it = s_childProcesses.begin(); it != s_childProcesses.end();) {
        std::set<ChildProcess*>::iterator current = it++;
        ChildProcess* child = *current;

        if (!WaitForSingleObjectEx(child->m_process, 0, FALSE)) {
            child->m_callback(child->m_param);
            s_childProcesses.erase(current);
            delete child;
        }
    }

    if (!s_childProcesses.size()) {
        KillTimer(nullptr, s_timer);
        s_timer = 0;
    }
}

// OFFSET: 0x00435C20
bool Blizzard::Process::GetProcessPath(char* buffer, int32_t buffersize) {
    if (!buffer || buffersize <= 0) {
        return false;
    }

    char filename[MAX_PATH];
    filename[0] = '\0';
    GetModuleFileNameA(GetModuleHandleA(nullptr), filename, MAX_PATH);
    System_File::FromNativeName(buffer, buffersize, filename);

    return true;
}

// OFFSET: 0x004389C0
bool Blizzard::Process::GetProcessDirectory(char* buffer, int32_t buffersize) {
    Blizzard::Memory::TempBuffer<char, 1024> path(buffersize);

    if (!GetProcessPath(path.m_data, buffersize)) {
        return false;
    }

    if (path.m_data) {
        if (buffer) {
            int32_t length = static_cast<int32_t>(Blizzard::String::FindFilename(path.m_data) - path.m_data + 1);
            Blizzard::String::Copy(buffer, path.m_data, std::min(length, buffersize));
        }
    } else if (buffer && buffersize > 0) {
        *buffer = '\0';
    }

    return true;
}

// OFFSET: 0x00440D80
bool Blizzard::Process::LaunchChildProcessWithArguments(const char* path, const char* arguments, void (*callback)(void*), void* param) {
    if (!arguments) {
        arguments = "";
    }

    std::string command;
    command.push_back('"');
    command.append(path);
    command.push_back('"');

    while (*arguments) {
        size_t length = strcspn(arguments, "\n");
        command.push_back(' ');

        bool quote = Blizzard::String::FindFirstOf(arguments, "\t ", static_cast<int32_t>(length)) != nullptr;

        if (quote) {
            command.push_back('"');
        }

        command.append(std::string(arguments, length));

        if (quote) {
            command.push_back('"');
        }

        arguments += length;
        arguments += strspn(arguments, "\n");
    }

    STARTUPINFOA startup;
    PROCESS_INFORMATION info;
    memset(&startup, 0, sizeof(startup));
    memset(&info, 0, sizeof(info));
    startup.cb = sizeof(startup);

    BOOL created;

    {
        Blizzard::StringConversion_CodepageToUTF8::ToNative nativeCommand(command.c_str());
        Blizzard::StringConversion_CodepageToUTF8::ToNative nativePath(path);
        created = CreateProcessA(nativePath.m_data, nativeCommand.m_data, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup, &info);
    }

    if (!created) {
        return false;
    }

    if (!callback) {
        return true;
    }

    if (WaitForInputIdle(info.hProcess, 10000)) {
        return false;
    }

    ChildProcess* child = new ChildProcess;
    child->m_process = info.hProcess;
    child->m_callback = callback;
    child->m_param = param;
    s_childProcesses.insert(child);

    if (!s_timer) {
        s_timer = SetTimer(nullptr, 0, 250, TimerFunc);
    }

    return true;
}

// INLINED
void Blizzard::Process::Sleep(uint32_t milliseconds) {
    ::Sleep(milliseconds);
}

