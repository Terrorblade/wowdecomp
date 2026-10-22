#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif

#include "HoT/MSDF/Os.h"
#include "HoT/Platform/Process.h"
#include <windows.h>

namespace HoT::MSDF::Os {

// The placeholder calls are Windows 10 1803 and later, so they are looked up at run time.
namespace PlaceholderVA {

using VirtualAlloc2Fn = PVOID (WINAPI*)(HANDLE, PVOID, SIZE_T, ULONG, ULONG, MEM_EXTENDED_PARAMETER*, ULONG);
using MapViewOfFile3Fn = PVOID (WINAPI*)(HANDLE, HANDLE, PVOID, ULONG64, SIZE_T, ULONG, ULONG, MEM_EXTENDED_PARAMETER*, ULONG);
using UnmapViewOfFile2Fn = BOOL (WINAPI*)(HANDLE, PVOID, ULONG);

static FARPROC KernelBaseProc(const char* name) {
    HMODULE module = GetModuleHandleW(L"kernelbase.dll");

    return module ? GetProcAddress(module, name) : nullptr;
}

static VirtualAlloc2Fn VirtualAlloc2P() {
    static VirtualAlloc2Fn fn = reinterpret_cast<VirtualAlloc2Fn>(KernelBaseProc("VirtualAlloc2"));
    return fn;
}

static MapViewOfFile3Fn MapViewOfFile3P() {
    static MapViewOfFile3Fn fn = reinterpret_cast<MapViewOfFile3Fn>(KernelBaseProc("MapViewOfFile3"));
    return fn;
}

static UnmapViewOfFile2Fn UnmapViewOfFile2P() {
    static UnmapViewOfFile2Fn fn = reinterpret_cast<UnmapViewOfFile2Fn>(KernelBaseProc("UnmapViewOfFile2"));
    return fn;
}

static bool Available() {
    return VirtualAlloc2P() && MapViewOfFile3P() && UnmapViewOfFile2P();
}

}

static HANDLE ToHandle(Handle handle) {
    return reinterpret_cast<HANDLE>(handle);
}

Handle OpenFile(const std::filesystem::path& path, Access access) {
    HANDLE file = INVALID_HANDLE_VALUE;

    switch (access) {
        case Access::ReadSequential:
            file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
            break;
        case Access::ReadRandom:
            file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS, nullptr);
            break;
        case Access::Append:
            file = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            break;
        case Access::Create:
            file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            break;
        case Access::CreateSequential:
            file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
            break;
    }

    return reinterpret_cast<Handle>(file);
}

void CloseFile(Handle file) {
    CloseHandle(ToHandle(file));
}

bool FileSize(Handle file, uint64_t& size) {
    LARGE_INTEGER value;

    if (!GetFileSizeEx(ToHandle(file), &value)) {
        return false;
    }

    size = static_cast<uint64_t>(value.QuadPart);
    return true;
}

bool WriteAll(Handle file, const void* data, size_t size) {
    DWORD written = 0;
    return WriteFile(ToHandle(file), data, static_cast<DWORD>(size), &written, nullptr) && written == size;
}

void* MapReadOnly(Handle file, size_t size) {
    HANDLE mapping = CreateFileMappingW(ToHandle(file), nullptr, PAGE_READONLY, 0, 0, nullptr);

    if (!mapping) {
        return nullptr;
    }

    void* view = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
    CloseHandle(mapping);
    return view;
}

void Unmap(void* view, size_t size) {
    UnmapViewOfFile(view);
}

size_t AllocationGranularity() {
    static const size_t s_granularity = [] {
        SYSTEM_INFO info;
        GetSystemInfo(&info);
        return static_cast<size_t>(info.dwAllocationGranularity);
    }();

    return s_granularity;
}

bool ArenaSupported() {
    return PlaceholderVA::Available() && !Platform::IsWine();
}

void* ReserveArena(size_t slotSize, size_t slotCount) {
    void* base = PlaceholderVA::VirtualAlloc2P()(GetCurrentProcess(), nullptr, slotSize * slotCount, MEM_RESERVE | MEM_RESERVE_PLACEHOLDER, PAGE_NOACCESS, nullptr, 0);

    if (!base) {
        return nullptr;
    }

    for (size_t i = 0; i < slotCount; i++) {
        VirtualFreeEx(GetCurrentProcess(), static_cast<char*>(base) + i * slotSize, slotSize, MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER);
    }

    return base;
}

void ReleaseArena(void* base, size_t size) {
    VirtualFreeEx(GetCurrentProcess(), base, 0, MEM_RELEASE);
}

void ResetSlot(void* slot, size_t slotSize) {
    uintptr_t current = reinterpret_cast<uintptr_t>(slot);
    uintptr_t end = current + slotSize;

    while (current < end) {
        MEMORY_BASIC_INFORMATION mbi;

        if (VirtualQuery(reinterpret_cast<void*>(current), &mbi, sizeof(mbi)) == 0) {
            break;
        }

        if (reinterpret_cast<uintptr_t>(mbi.BaseAddress) >= end) {
            break;
        }

        if (mbi.State != MEM_FREE) {
            if (mbi.Type == MEM_MAPPED) {
                PlaceholderVA::UnmapViewOfFile2P()(GetCurrentProcess(), mbi.BaseAddress, 0);
            } else {
                VirtualFree(mbi.BaseAddress, 0, MEM_RELEASE);
            }
        }

        current += mbi.RegionSize;
    }

    VirtualFreeEx(GetCurrentProcess(), slot, 0, MEM_RELEASE | MEM_COALESCE_PLACEHOLDERS);
    PlaceholderVA::VirtualAlloc2P()(GetCurrentProcess(), slot, slotSize, MEM_RESERVE | MEM_RESERVE_PLACEHOLDER, PAGE_NOACCESS, nullptr, 0);
}

void* MapIntoSlot(Handle file, void* slot, size_t size, Handle& mapping) {
    if (!VirtualFreeEx(GetCurrentProcess(), slot, static_cast<SIZE_T>(size), MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER)) {
        return nullptr;
    }

    uint64_t size64 = size;
    HANDLE section = CreateFileMappingW(ToHandle(file), nullptr, PAGE_READONLY, static_cast<DWORD>(size64 >> 32), static_cast<DWORD>(size64 & 0xFFFFFFFF), nullptr);

    if (!section) {
        return nullptr;
    }

    mapping = reinterpret_cast<Handle>(section);

    return PlaceholderVA::MapViewOfFile3P()(section, nullptr, slot, 0, static_cast<SIZE_T>(size), MEM_REPLACE_PLACEHOLDER, PAGE_READONLY, nullptr, 0);
}

void UnmapFromSlot(void* view, size_t size) {
    if (PlaceholderVA::UnmapViewOfFile2Fn unmap = PlaceholderVA::UnmapViewOfFile2P()) {
        unmap(GetCurrentProcess(), view, MEM_PRESERVE_PLACEHOLDER);
    }
}

void CloseMapping(Handle mapping) {
    if (mapping) {
        CloseHandle(ToHandle(mapping));
    }
}

}
