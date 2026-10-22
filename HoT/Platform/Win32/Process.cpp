#include "HoT/Platform/Process.h"
#include <string>
#include <vector>
#include <windows.h>
#include <psapi.h>

namespace HoT::Platform {

std::filesystem::path ExecutablePath() {
    std::vector<wchar_t> buffer(MAX_PATH);

    for (;;) {
        DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));

        if (length == 0) {
            return std::filesystem::path();
        }

        if (length < buffer.size()) {
            return std::filesystem::path(std::wstring(buffer.data(), length));
        }

        buffer.resize(buffer.size() * 2);
    }
}

std::filesystem::path GameDirectory() {
    std::filesystem::path exe = ExecutablePath();

    if (exe.empty()) {
        std::error_code ec;
        return std::filesystem::current_path(ec);
    }

    return exe.parent_path();
}

bool IsRunningExecutable(const std::filesystem::path& path) {
    std::filesystem::path exe = ExecutablePath();

    if (exe.empty()) {
        return false;
    }

    std::error_code ec;
    return std::filesystem::equivalent(path, exe, ec);
}

uint32_t CurrentProcessId() {
    return GetCurrentProcessId();
}

uint64_t CurrentThreadId() {
    return GetCurrentThreadId();
}

void LowerThreadPriority() {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
}

void SetBackgroundThreadPriority() {
    SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_BEGIN);
}

bool IsWine() {
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");

    return ntdll && GetProcAddress(ntdll, "wine_get_version") != nullptr;
}

bool ProcessMemory(uint64_t& workingSet, uint64_t& privateBytes) {
    PROCESS_MEMORY_COUNTERS_EX memory = {};
    memory.cb = sizeof(memory);

    if (!GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory), sizeof(memory))) {
        return false;
    }

    workingSet = memory.WorkingSetSize;
    privateBytes = memory.PrivateUsage;
    return true;
}

}
