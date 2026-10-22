#ifndef HOT_PLATFORM_PROCESS_H
#define HOT_PLATFORM_PROCESS_H

#include <cstdint>
#include <filesystem>

namespace HoT::Platform {

// The running binary, empty if it cannot be found.
std::filesystem::path ExecutablePath();

// The game folder: the exe folder, or on macOS the folder holding the app bundle.
std::filesystem::path GameDirectory();

bool IsRunningExecutable(const std::filesystem::path& path);

uint32_t CurrentProcessId();
uint64_t CurrentThreadId();

// BELOW_NORMAL for the calling thread.
void LowerThreadPriority();

// Background mode for the calling thread, for long running IO.
void SetBackgroundThreadPriority();

bool IsWine();

// Working set and private bytes of this process.
bool ProcessMemory(uint64_t& workingSet, uint64_t& privateBytes);

}

#endif
